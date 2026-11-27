"""Link a relocatable CoreS3 game against the StackCube SDK."""

import os
from pathlib import Path
import re
import subprocess

from SCons.Script import Import

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
sdk = project.parents[1] / "StackCubeAPI" / "src"
compiler = Path(env.WhereIs(env.subst("$CXX")))
nm = compiler.with_name(compiler.name.replace("-g++", "-nm"))
system_env = os.environ.copy()
system_env["PATH"] = env["ENV"]["PATH"]
lvgl = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "lvgl"
lvgl_config = project.parents[1] / "StackCubeConsole" / "include"
exports = sdk / "StackCubeLvglExports.h"
supported_imports = {"memcpy", "memmove", "memset", "snprintf"}
supported_imports.update(re.findall(r"^STACKCUBE_LVGL_EXPORT\((\w+)\)$",
                                    exports.read_text(encoding="utf-8"), re.MULTILINE))


def link_game(target, source, env):
    output = Path(target[0].get_abspath())
    output.parent.mkdir(parents=True, exist_ok=True)
    link = [str(compiler), "-std=gnu++17", "-Os", "-fPIC", "-mlongcalls", "-fvisibility=hidden",
            "-fno-exceptions", "-fno-rtti", "-fno-use-cxa-atexit", "-ffunction-sections",
            "-fdata-sections", "-nostdlib", "-shared", "-Wl,-e,stackcube_game_entry",
            "-Wl,--gc-sections,--strip-all", "-DLV_CONF_INCLUDE_SIMPLE",
            "-I", str(sdk), "-I", str(lvgl), "-I", str(lvgl_config), "-o", str(output)]
    link.extend(node.get_abspath() for node in source if node.suffix == ".cpp")
    subprocess.run([*link, "-lgcc"], env=system_env, check=True)
    undefined = subprocess.check_output(
        [str(nm), "-D", "--undefined-only", str(output)], env=system_env, text=True)
    missing = {line.split()[-1] for line in undefined.splitlines()} - supported_imports
    if missing:
        output.unlink()
        raise RuntimeError(f"Unsupported game imports: {', '.join(sorted(missing))}")


sources = Path(env.subst("$PROJECT_SRC_DIR"))
inputs = sorted(path for root in (sources, sdk) for path in root.rglob("*")
                if path.suffix in (".cpp", ".h"))
module = env.Command("$BUILD_DIR/game.elf", [str(path) for path in inputs], link_game)
env.Depends(module, str(project.parent / "build_game.py"))
env.Depends(module, str(lvgl_config / "lv_conf.h"))
env.AddCustomTarget("game", module, None, title="Build StackCube game")

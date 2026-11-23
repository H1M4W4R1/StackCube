"""Select the INT wiring in HARDWARE.MD in the pinned USB Host Shield library."""
from pathlib import Path
from SCons.Script import Import

Import("env")

dependency_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
headers = list(dependency_dir.glob("*/UsbCore.h"))
if len(headers) != 1:
    raise RuntimeError("Expected one USB Host Shield UsbCore.h in the project dependencies")

def replace_once(path, original, wiring):
    source = path.read_bytes()
    if original in source:
        path.write_bytes(source.replace(original, wiring, 1))
    elif wiring not in source:
        raise RuntimeError(f"USB Host Shield CoreS3 pin definition changed in {path.name}")


header = headers[0]
replace_once(
    header.with_name("avrpins.h"),
    b"MAKE_PIN(P14, 14); // INT",
    b"MAKE_PIN(P10, 10); // StackCube CoreS3 INT",
)
replace_once(
    header,
    b"typedef MAX3421e<P1, P14> MAX3421E; // M5Stack Core S3",
    b"typedef MAX3421e<P1, P10> MAX3421E; // StackCube CoreS3",
)

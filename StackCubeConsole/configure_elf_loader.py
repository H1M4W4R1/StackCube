"""Build the pinned Espressif ELF loader with the Arduino SDK."""

from hashlib import sha256
from io import BytesIO
from pathlib import Path
from urllib.request import urlopen
from zipfile import ZipFile

from SCons.Script import Import

Import("env")

version = "2.1.0"
archive_name = f"esp-elf-loader-{version}"
url = (f"https://components-file.espressif.com/components/espressif/elf_loader/{version}"
       "/download/99d041a4-1cfd-48c3-b99d-3e412b3e2ff2.zip")
digest = "31a00c76ae203d2f29d257bde2c26fb0d79cb0ee46e3b67e61baa361f01a8e4c"
cache = Path(env.subst("$PROJECT_DIR")) / ".pio"
archive = cache / f"{archive_name}.zip"
component = cache / archive_name / "elf_loader"
sources = ["esp_elf.c", "esp_elf_validate.c", "esp_elf_symbol.c",
           "esp_elf_adapter.c", "arch/esp_elf_xtensa.c"]

if not (component / "include" / "esp_elf.h").is_file():
    if archive.is_file():
        content = archive.read_bytes()
    else:
        with urlopen(url, timeout=30) as response:
            content = response.read()
    if sha256(content).hexdigest() != digest:
        raise RuntimeError(f"Unexpected checksum for Espressif ELF loader {version}")
    cache.mkdir(parents=True, exist_ok=True)
    archive.write_bytes(content)
    with ZipFile(BytesIO(content)) as package:
        members = [f"elf_loader/src/{name}" for name in sources]
        members.extend(name for name in package.namelist()
                       if name.startswith("elf_loader/include/"))
        members.append("elf_loader/license.txt")
        package.extractall(component.parent, members)

# Arduino 2's Xtensa toolchain rejects the C11 atomic function-pointer typedef.
# Its GCC atomic builtins keep the same sequentially consistent access.
resolver_source = component / "src" / "esp_elf.c"
original = resolver_source.read_text(encoding="utf-8")
patched = original.replace("_Atomic(symbol_resolver)", "symbol_resolver")
patched = patched.replace("atomic_load(&current_resolver)",
                          "__atomic_load_n(&current_resolver, __ATOMIC_SEQ_CST)")
patched = patched.replace("atomic_store(&current_resolver, resolver)",
                          "__atomic_store_n(&current_resolver, resolver, __ATOMIC_SEQ_CST)")
patched = patched.replace("atomic_store(&current_resolver, elf_find_sym_default)",
                          "__atomic_store_n(&current_resolver, elf_find_sym_default, __ATOMIC_SEQ_CST)")
# The allocation is word-aligned separately. Keep the exact .text length for
# symbol lookup so adjacent .rodata goes to the byte-readable PSRAM alias.
patched = patched.replace("elf->sec[ELF_SEC_TEXT].size    = ELF_ALIGN(text_size, 4);",
                          "elf->sec[ELF_SEC_TEXT].size    = text_size;")
if patched != original:
    resolver_source.write_text(patched, encoding="utf-8")

env.Append(CPPPATH=[str(component / "include")], CPPDEFINES=[
    "CONFIG_ELF_LOADER", "CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR",
    "CONFIG_ELF_LOADER_LOAD_PSRAM", "CONFIG_ELF_LOADER_CACHE_OFFSET",
    *[(f"ELF_LOADER_VER_{part}", number)
      for part, number in zip(("MAJOR", "MINOR", "PATCH"), version.split("."))],
])
env.BuildSources("$BUILD_DIR/elf_loader", str(component / "src"),
                 src_filter=[f"+<{name}>" for name in sources])

"""A launcher upload must boot the factory app even if a game is already cached."""
from pathlib import Path
from SCons.Script import Import

Import("env")

otadata = Path(env.subst("$BUILD_DIR")) / "factory-otadata.bin"
otadata.parent.mkdir(parents=True, exist_ok=True)
erased = bytes([0xff]) * 0x2000
if not otadata.exists() or otadata.read_bytes() != erased:
    otadata.write_bytes(erased)

images = env["FLASH_EXTRA_IMAGES"]
env.Replace(FLASH_EXTRA_IMAGES=[
    (offset, str(otadata) if int(offset, 0) == 0xe000 else image)
    for offset, image in images
])
flags = env.get("UPLOADERFLAGS", [])
env.Replace(UPLOADERFLAGS=[
    str(otadata) if str(flag).endswith("boot_app0.bin") else flag for flag in flags
])

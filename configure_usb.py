"""Configure the pinned USB Host Shield library for StackCube."""
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
        raise RuntimeError(f"Pinned USB Host Shield source changed in {path.name}")


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

# The connection buzz runs before the user callback. Keep the input handshake and
# callback, and omit the output packet that activates the motors.
xbox = header.with_name("XBOXONE.cpp")
source = xbox.read_bytes()
newline = b"\r\n" if b"\r\n" in source else b"\n"
start = b"void XBOXONE::onInit() {" + newline
callback = b"        if(pFuncOnInit)" + newline
prefix, has_start, init = source.partition(start)
buzz, has_callback, continuation = init.partition(callback)
if not has_start or not has_callback or (buzz and b"XboxCommand(writeBuf, 13);" not in buzz):
    raise RuntimeError("Pinned USB Host Shield Xbox One startup routine changed")
if buzz:
    xbox.write_bytes(prefix + start + callback + continuation)

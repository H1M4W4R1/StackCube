"""Configure the pinned USB Host Shield library for StackCube."""
from pathlib import Path
from SCons.Script import Import

Import("env")

dependency_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
headers = list(dependency_dir.glob("*/UsbCore.h"))
if len(headers) != 1:
    raise RuntimeError("Expected one USB Host Shield UsbCore.h in the project dependencies")

def replace_once(path, original, wiring, previous=None):
    source = path.read_bytes()
    if wiring in source:
        return
    if previous is not None and previous in source:
        original = previous
    if original not in source:
        raise RuntimeError(f"Pinned USB Host Shield source changed in {path.name}")
    path.write_bytes(source.replace(original, wiring, 1))


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


def patch_xbox(original, replacement, previous=None):
    replace_once(xbox, original.replace(b"\n", newline), replacement.replace(b"\n", newline),
                 previous.replace(b"\n", newline) if previous else None)


# Expose the USB driver's report stream and output path to StackCube's GIP session handler.
on_init = b"        onInit();\n        XboxOneConnected = true;"
old_init = (
    b"        if (VID == XBOX_VID1 && PID == XBOX_ONE_PID4) {\n"
    b"                uint8_t wiredMode[] = {0x05, 0x20, 0x00, 0x0f, 0x06};\n"
    b"                rcode = XboxCommand(wiredMode, sizeof(wiredMode));\n"
    b"                if (rcode)\n"
    b"                        goto Fail;\n"
    b"        }\n\n"
    + on_init
)
patch_xbox(
    on_init,
    b"        rcode = StartInputSession(VID, PID);\n"
    b"        if (rcode)\n"
    b"                goto Fail;\n\n" + on_init,
    old_init,
)

xbox_header = header.with_name("XBOXONE.h")
header_newline = b"\r\n" if b"\r\n" in xbox_header.read_bytes() else b"\n"
xbox_command = b"        uint8_t XboxCommand(uint8_t* data, uint16_t nbytes);"
replace_once(
    xbox_header,
    b"        /* Private commands */" + header_newline + xbox_command,
    (b"protected:\n"
     b"        virtual uint8_t StartInputSession(uint16_t, uint16_t) { return 0; }\n"
     b"        virtual void OnInputPacket(const uint8_t*, uint16_t) {}\n"
     b"        void ResetInputState();\n" + xbox_command).replace(b"\n", header_newline),
)

# Preserve USB errors in non-debug builds and report actual results to the host.
for diagnostic in (b"NotifyFailGetDevDescr", b"NotifyFailSetDevTblEntry"):
    patch_xbox(
        b"        " + diagnostic + b"();\n        goto Fail;\n#endif",
        b"        " + diagnostic + b"();\n#endif\n        goto Fail;",
    )
patch_xbox(b"uint8_t rcode = pUsb->inTransfer(bAddress, epInfo[ XBOX_ONE_INPUT_PIPE ]",
           b"rcode = pUsb->inTransfer(bAddress, epInfo[ XBOX_ONE_INPUT_PIPE ]")
parse_report = (
    b"                if(!rcode && length > 0 &&\n"
    b"                   ((readBuf[0] == 0x07 && length >= 5) || (readBuf[0] == 0x20 && length >= 18))) {\n"
    b"                        if (length <= 22) readBuf[22] = 0;\n"
    b"                        readReport();"
)
patch_xbox(b"                if(!rcode) {\n                        readReport();",
           b"                if (!rcode && length) OnInputPacket(readBuf, length);\n" + parse_report,
           parse_report)

reset_input = (
    b"        ButtonState = OldButtonState = ButtonClickState = 0;\n"
    b"        memset(hatValue, 0, sizeof(hatValue));\n"
    b"        memset(triggerValue, 0, sizeof(triggerValue));\n"
    b"        memset(triggerValueOld, 0, sizeof(triggerValueOld));\n"
    b"        L2Clicked = R2Clicked = sharePressed = shareClicked = false;\n"
)
release = b"uint8_t XBOXONE::Release() {\n        XboxOneConnected = false;\n"
address_pool = b"        pUsb->GetAddressPool()"
patch_xbox(release + address_pool,
           b"void XBOXONE::ResetInputState() {\n" + reset_input + b"}\n\n" + release
           + b"        ResetInputState();\n" + address_pool,
           release + reset_input + address_pool)

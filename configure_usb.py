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


def patch_usb_source(filename, original, replacement):
    path = header.with_name(filename)
    eol = b"\r\n" if b"\r\n" in path.read_bytes() else b"\n"
    replace_once(path, original.replace(b"\n", eol), replacement.replace(b"\n", eol))


# A powered hub can keep its downstream devices addressed across an ESP restart.
# Scan and reset occupied ports once the new host has configured the hub.
hub_poll_flag = b"        bool bPollEnable; // poll enable flag"
patch_usb_source("usbhub.h",
    hub_poll_flag,
    hub_poll_flag + b"\n        uint32_t nextPortScan = 0;\n"
    b"        uint8_t startupPorts = 0, startupRetried = 0;")
patch_usb_source("usbhub.cpp",
    b"        uint16_t cd_len = 0;",
    b"        uint16_t cd_len = 0;\n"
    b"        uint16_t powerGoodMs = 0;")
patch_usb_source("usbhub.cpp",
    b"        bNbrPorts = hd->bNbrPorts;",
    b"        bNbrPorts = hd->bNbrPorts;\n"
    b"        powerGoodMs = 2u * hd->bPwrOn2PwrGood;")
hub_port_power = (
    b"        for(uint8_t j = 1; j <= bNbrPorts; j++)\n"
    b"                SetPortFeature(HUB_FEATURE_PORT_POWER, j, 0); //HubPortPowerOn(j);\n\n"
)
patch_usb_source("usbhub.cpp",
    hub_port_power + b"        pUsb->SetHubPreMask();",
    hub_port_power + b"        startupPorts = (1u << (bNbrPorts < 7 ? bNbrPorts + 1 : 8)) - 2;\n"
    b"        startupRetried = 0;\n"
    b"        qNextPollTime = nextPortScan = (uint32_t)millis() + powerGoodMs;\n"
    b"        pUsb->SetHubPreMask();")
# Finish each startup reset and enumeration before resetting another port.
# A powered hub may supply neither a connection event nor a new reset bitmap.
patch_usb_source("usbhub.cpp",
    b"uint8_t USBHub::CheckHubStatus() {\n        uint8_t rcode;",
    b"uint8_t USBHub::CheckHubStatus() {\n"
    b"        for (uint8_t port = 1; startupPorts && !bResetInitiated && port < 8; ++port) {\n"
    b"                const uint8_t bit = 1u << port;\n"
    b"                if (!(startupPorts & bit)) continue;\n"
    b"                HubEvent evt = {};\n"
    b"                if (GetPortStatus(port, sizeof(evt), evt.evtBuff)) continue;\n"
    b"                startupPorts &= ~bit;\n"
    b"                if (!(evt.bmStatus & bmHUB_PORT_STATUS_PORT_CONNECTION)) continue;\n"
    b"                ResetHubPort(port);\n"
    b"                uint8_t result = GetPortStatus(port, sizeof(evt), evt.evtBuff);\n"
    b"                const uint16_t enabled = bmHUB_PORT_STATUS_PORT_CONNECTION | bmHUB_PORT_STATUS_PORT_ENABLE;\n"
    b"                if (!result && (evt.bmStatus & enabled) == enabled) {\n"
    b"                        UsbDeviceAddress address;\n"
    b"                        address.devAddress = bAddress;\n"
    b"                        result = pUsb->Configuring(address.bmAddress, port,\n"
    b"                            (evt.bmStatus & bmHUB_PORT_STATUS_PORT_LOW_SPEED) != 0);\n"
    b"                }\n"
    b"                if ((result || (evt.bmStatus & enabled) != enabled) && !(startupRetried & bit)) {\n"
    b"                        startupRetried |= bit;\n"
    b"                        startupPorts |= bit;\n"
    b"                }\n"
    b"                return result;\n"
    b"        }\n\n"
    b"        uint8_t rcode;")
patch_usb_source("usbhub.cpp",
    b"        if(bAddress == 0x41)\n                pUsb->SetHubPreMask();",
    b"        if(bAddress == 0x41) {\n"
    b"                pUsb->ResetHubPreMask();\n"
    b"                bResetInitiated = false;\n"
    b"        }")

# A connected port can stay disabled without a change notification. Poll it
# periodically even when the status endpoint has no change bitmap to report.
patch_usb_source("usbhub.cpp",
    b"        rcode = pUsb->inTransfer(bAddress, 1, &read, buf);\n\n"
    b"        if(rcode)\n                return rcode;",
    b"        rcode = pUsb->inTransfer(bAddress, 1, &read, buf);\n"
    b"        if (rcode && rcode != hrNAK) return rcode;\n"
    b"        const uint32_t now = millis();\n"
    b"        if (rcode == hrNAK || !read) {\n"
    b"                if (int32_t(now - nextPortScan) < 0) return 0;\n"
    b"                buf[0] = 0;\n"
    b"        }\n"
    b"        nextPortScan = now + 500;")
patch_usb_source("usbhub.cpp",
    b"uint8_t USBHub::PortStatusChange(uint8_t port, HubEvent &evt) {\n"
    b"        switch(evt.bmEvent) {",
    b"uint8_t USBHub::PortStatusChange(uint8_t port, HubEvent &evt) {\n"
    b"        const bool connected = evt.bmStatus & bmHUB_PORT_STATUS_PORT_CONNECTION;\n"
    b"        const bool lowSpeed = evt.bmStatus & bmHUB_PORT_STATUS_PORT_LOW_SPEED;\n"
    b"        uint32_t event = 0;\n"
    b"        if (!connected && (evt.bmChange & bmHUB_PORT_STATUS_C_PORT_CONNECTION)) {\n"
    b"                event = bmHUB_PORT_EVENT_DISCONNECT;\n"
    b"        } else if (connected && (evt.bmStatus & bmHUB_PORT_STATUS_PORT_ENABLE) &&\n"
    b"                   (evt.bmChange & bmHUB_PORT_STATUS_C_PORT_RESET)) {\n"
    b"                event = lowSpeed ? bmHUB_PORT_EVENT_LS_RESET_COMPLETE : bmHUB_PORT_EVENT_RESET_COMPLETE;\n"
    b"        } else if (connected && (evt.bmChange & bmHUB_PORT_STATUS_C_PORT_CONNECTION) &&\n"
    b"                   !(evt.bmStatus & (bmHUB_PORT_STATUS_PORT_ENABLE | bmHUB_PORT_STATUS_PORT_RESET |\n"
    b"                                     bmHUB_PORT_STATUS_PORT_SUSPEND))) {\n"
    b"                event = lowSpeed ? bmHUB_PORT_EVENT_LS_CONNECT : bmHUB_PORT_EVENT_CONNECT;\n"
    b"        }\n"
    b"        switch(event) {")
patch_usb_source("usbhub.cpp",
    b"                        SetPortFeature(HUB_FEATURE_PORT_RESET, port, 0);\n"
    b"                        bResetInitiated = true;",
    b"                        ClearPortFeature(HUB_FEATURE_C_PORT_RESET, port, 0);\n"
    b"                        if (const uint8_t result = SetPortFeature(HUB_FEATURE_PORT_RESET, port, 0))\n"
    b"                                return result;\n"
    b"                        startupPorts &= ~(1u << port);\n"
    b"                        bResetInitiated = true;")

# The parent identifier is the hub number encoded in its USB address.
# Driver registration order is independent of that number.
patch_usb_source("Usb.cpp",
    b"        uint8_t retries = 0;\n\nagain:",
    b"        uint8_t retries = 0;\n"
    b"        const auto resetParentPort = [this, parent, port]() {\n"
    b"                for (auto* driver : devConfig) {\n"
    b"                        if (!driver) continue;\n"
    b"                        UsbDeviceAddress address;\n"
    b"                        address.devAddress = driver->GetAddress();\n"
    b"                        if (address.bmHub && address.bmAddress == parent) {\n"
    b"                                driver->ResetHubPort(port);\n"
    b"                                return;\n"
    b"                        }\n"
    b"                }\n"
    b"        };\n\nagain:")
usb_core = header.with_name("Usb.cpp")
usb_core_source = usb_core.read_bytes()
old_reset_call = b"devConfig[parent]->ResetHubPort(port);"
reset_call = b"resetParentPort();"
if usb_core_source.count(old_reset_call) == 2:
    usb_core.write_bytes(usb_core_source.replace(old_reset_call, reset_call))
elif usb_core_source.count(reset_call) != 2:
    raise RuntimeError("Pinned USB Host Shield parent hub reset calls changed")

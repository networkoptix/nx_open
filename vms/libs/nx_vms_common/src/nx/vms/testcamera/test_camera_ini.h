// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::testcamera {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("test_camera.ini") { reload(); }

    NX_INI_INT(4984, discoveryPort, "Default port (used when the corresponding command-line\n"
        "option is not specified) on which testcamera expects discovery messages from Servers\n"
        "which have camera auto-discovery feature enabled.");

    NX_INI_INT(4985, mediaPort, "Default port (used when the corresponding command-line\n"
        "option is not specified) on which testcamera serves the media stream.");

    NX_INI_STRING(
        "Nx Testcamera discovery",
        discoveryMessage,
        "Message expected from the Server when discovering testcamera. Must be specified without\n"
        "a trailing newline.");

    NX_INI_STRING(
        "Nx Testcamera discovery response",
        discoveryResponseMessagePrefix,
        "Prefix for the message that is sent by testcamera to the Server when the Camera gets\n"
        "discovered. Must be specified without a trailing newline.");

    NX_INI_FLAG(false, printOptions, "Print parsed command-line options to stderr.");

    NX_INI_FLAG(true, stopStreamingOnErrors,
        "Stop streaming from the particular camera on any error while streaming a file.");

    NX_INI_FLAG(true, obtainFramePtsFromTimestampField,
        "If set, frame PTS to be sent to the Server is obtained from `timestamp` field instead\n"
        "of `pts` field of the QnCompressedVideoData object returned by QnAviArchiveDelegate.");

    NX_INI_FLAG(true, warnIfFramePtsDiffersFromTimestamp,
        "Whether to log a warning for each frame which has different values in `pts` and\n"
        "`timestamp` fields the QnCompressedVideoData object returned by QnAviArchiveDelegate.\n"
        "This is expected to happen when B-frames (reordered frames) are present in the stream.");

    NX_INI_INT(100, discoveryMessageTimeoutMs,
        "Timeout for reading Camera discovery message from the socket, in milliseconds.");

    NX_INI_FLAG(true, logReceivingDiscoveryMessageErrorAsVerbose,
        "Whether to log errors of receiving discovery messages using VERBOSE log level. If not\n"
        "set, log them using ERROR log level.");

    NX_INI_FLAG(false, declareTwoWayAudio,
        "Whether testcamera must declare itself as capable of receiving the audio from Client.\n"
        "ATTENTION: Must be set in the .ini file of the Server, not the one of the testcamera.");

    NX_INI_FLAG(false, ignoreUsb0NetworkInterfaceIfOthersExist,
        "Influence enumerating local network interfaces when listening to discovery messages.\n"
        "The option works the same way as the option in the Server settings with the same name.\n"
        "If set, when enumerating network interfaces, the Server will skip the \"usb0\"\n"
        "interface in case it is not the only interface. Can be useful for some ARM devices.");
};

NX_VMS_COMMON_API Ini& ini();

} // namespace nx::vms::testcamera

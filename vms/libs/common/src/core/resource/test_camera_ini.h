#pragma once

#include <nx/kit/ini_config.h>

struct TestCameraIni: public nx::kit::IniConfig
{
    TestCameraIni(): IniConfig("test_camera.ini") { reload(); }

    NX_INI_STRING("92-61", macPrefix, "First two bytes of the desired MAC, separeted by '-'.");

    NX_INI_INT(4984, discoveryPort, "Port on which test camera expects discovery packets used\n"
        "for Server's auto camera discovery feature.");

    NX_INI_INT(4985, mediaPort, "Port on which test camera serves the media stream.");

    NX_INI_STRING(
        "Network Optix Camera Emulator 3.0 discovery",
        discoveryMessage,
        "Message expected from the Server when discovering testcamera. Must be specified without\n"
        "a trailing newline.");

    NX_INI_STRING(
        "Network Optix Camera Emulator 3.0 discovery response",
        discoveryResponseMessage,
        "Message that is sent by testcamera to the Server when the Camera gets discovered. Must\n"
        "be specified without a trailing newline.");

    NX_INI_STRING("", logFramesFile, "If not empty, log video frames to this file.");

    NX_INI_FLAG(0, printOptions, "Print parsed command-line options to stderr.");

    NX_INI_FLAG(1, stopStreamingOnErrors,
        "Stop streaming from the particular camera on any error while streaming a file.");

    NX_INI_FLAG(1, obtainFramePtsFromTimestampField,
        "If set, frame PTS to be sent to the Server is obtained from `timestamp` field instead\n"
        "of `pts` field of the QnCompressedVideoData object returned by QnAviArchiveDelegate.");

    NX_INI_FLAG(1, warnIfFramePtsDiffersFromTimestamp,
        "Whether to log a warning for each frame which has different values in `pts` and\n"
        "`timestamp` fields the QnCompressedVideoData object returned by QnAviArchiveDelegate.\n"
        "This is expected to happen when B-frames (reordered frames) are present in the stream.");

    NX_INI_INT(100, discoveryMessageTimeoutMs,
        "Timeout for reading Camera discovery message from the socket, in milliseconds.");

    NX_INI_FLAG(1, logReceivingDiscoveryMessageErrorAsVerbose,
        "Whether to log errors of receiving discovery messages using VERBOSE log level. If set,\n"
        "log them using ERROR log level.");
};

inline TestCameraIni& testCameraIni()
{
    static TestCameraIni ini;
    return ini;
}

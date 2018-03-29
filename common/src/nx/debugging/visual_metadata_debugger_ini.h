#pragma once

#include <nx/kit/debug.h>
#include <nx/kit/ini_config.h>

namespace nx {
namespace debugging {

struct VisualMetadataDebuggerIni : public nx::kit::IniConfig
{
    VisualMetadataDebuggerIni() : nx::kit::IniConfig("visual_metadata_debugger.ini")
    {
        reload();
    }

    NX_INI_FLAG(0, enableOutput, "Enable debug output.");

    // Manager pool debugger settings
    NX_INI_FLAG(
        0,
        enableManagerPoolDebuggerInstance,
        "Enable visual debugger for metadata manager pool (plugins input/output).");

    NX_INI_STRING(
        "",
        managerPoolDebugOutputDirectory,
        "Directory for manager pool visual debugger instance ouput.");

    NX_INI_INT(
        100,
        managerPoolDebuggerFrameCacheSize,
        "Frame cache size of manager pool visual metadata debugger.");

    NX_INI_INT(
        100,
        managerPoolDebuggerMetadataCacheSize,
        "Metadata cache size of manager pool visual metadata debugger.");

    // Archive stream reader debugger settings
    NX_INI_FLAG(
        0,
        enableArchiveStreamReaderDebuggerInstance,
        "Enable visual debugger for archive stream reader.");

    NX_INI_STRING(
        "",
        archiveStreamReaderDebugOutputDirectory,
        "Directory for archive stream reader visual debugger instance ouput.");

    NX_INI_INT(
        100,
        archiveStreamReaderDebuggerFrameCacheSize,
        "Frame cache size of archive stream reader visual metadata debugger.");

    NX_INI_INT(
        100,
        archiveStreamReaderDebuggerMetadataCacheSize,
        "Metadata cache size of archive stream reader visual metadata debugger.");

    // Live connection debugger settings.
    NX_INI_FLAG(
        0,
        enableLiveConnectionDebuggerInstance,
        "Enable visual debugger for server side live connection output.");

    NX_INI_STRING(
        "",
        liveConnectionDebugOutputDirectory,
        "Directory for server side live coonnection visual debugger instance ouput.");

    NX_INI_INT(
        100,
        liveConnectionDebuggerFrameCacheSize,
        "Frame cache size of live connection visual metadata debugger.");

    NX_INI_INT(
        100,
        liveConnectionDebuggerMetadataCacheSize,
        "Metadata cache size of live connection visual metadata debugger.");

    // Nx RTP parser debugger settings.
    NX_INI_FLAG(
        0,
        enableNxRtpParserDebuggerInstance,
        "Enable visual debugger for Nx RTP parser.");

    NX_INI_STRING(
        "",
        nxRtpParserDebugOutputDirectory,
        "Directory for Nx RTP parser visual debugger instance output.");

    NX_INI_INT(
        100,
        nxRtpParserDebuggerFrameCacheSize,
        "Frame cache size of Nx RTP parser visual metadata debugger.");

    NX_INI_INT(
        100,
        nxRtpParserDebuggerMetadataCacheSize,
        "Metadata cache size of Nx RTP parser visual metadata debugger.");

};

inline VisualMetadataDebuggerIni& visualDebuggerIni()
{
    static VisualMetadataDebuggerIni ini;
    return ini;
}

} // namespace debugging
} // namespace nx
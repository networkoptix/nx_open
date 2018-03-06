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
    NX_INI_FLAG(
        0,
        enableManagerPoolDebuggerInstance,
        "Enable visual debugger for metadata manager pool (plugins input/output).");

    NX_INI_STRING(
        "",
        managerPoolDebugOutputDirectory,
        "Directory for manager pool visual debugger instance ouput.");

    NX_INI_FLAG(
        0,
        enableArchiveStreamReaderDebuggerInstance,
        "Enable visual debugger for archive stream reader.");

    NX_INI_STRING(
        "",
        archiveStreamReaderDebugOutputDirectory,
        "Directory for archive stream reader visual debugger instance ouput.");

    NX_INI_FLAG(
        0,
        enableLiveConnectionDebuggerInstance,
        "Enable visual debugger for server side live connection output.");

    NX_INI_STRING(
        "",
        managerPoolDebugOutputDirectory,
        "Directory for server side live coonnection visual debugger instance ouput.");
};

inline VisualMetadataDebuggerIni& visualDebuggerIni()
{
    static VisualMetadataDebuggerIni ini;
    return ini;
}

} // namespace debugging
} // namespace nx
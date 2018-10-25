#include "visual_metadata_debugger_factory.h"

#include <QtCore/QDir>

#include "visual_metadata_debugger.h"
#include "fake_visual_metadata_debugger.h"
#include "visual_metadata_debugger_ini.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace debugging {

VisualMetadataDebuggerPtr VisualMetadataDebuggerFactory::makeDebugger(
    DebuggerType debuggerType)
{
    bool isEnabled = false;
    QString path;
    int frameCacheSize = 0;
    int metadataCacheSize = 0;

    switch (debuggerType)
    {
        case DebuggerType::analyticsManager:
        {
            isEnabled = visualDebuggerIni().enableAnalyticsManagerDebuggerInstance;
            path = QString::fromUtf8(visualDebuggerIni().analyticsManagerDebugOutputDirectory);
            frameCacheSize = visualDebuggerIni().analyticsManagerDebuggerFrameCacheSize;
            metadataCacheSize = visualDebuggerIni().analyticsManagerDebuggerMetadataCacheSize;
            return makeDebuggerInternal(isEnabled, path, frameCacheSize, metadataCacheSize);
        }
        case DebuggerType::archiveStreamReader:
        {
            isEnabled = visualDebuggerIni().enableArchiveStreamReaderDebuggerInstance;
            path = QString::fromUtf8(visualDebuggerIni().archiveStreamReaderDebugOutputDirectory);
            frameCacheSize = visualDebuggerIni().archiveStreamReaderDebuggerFrameCacheSize;
            metadataCacheSize = visualDebuggerIni().archiveStreamReaderDebuggerMetadataCacheSize;
            return makeDebuggerInternal(isEnabled, path, frameCacheSize, metadataCacheSize);
        }
        case DebuggerType::liveConnection:
        {
            isEnabled = visualDebuggerIni().enableLiveConnectionDebuggerInstance;
            path = QString::fromUtf8(visualDebuggerIni().liveConnectionDebugOutputDirectory);
            frameCacheSize = visualDebuggerIni().liveConnectionDebuggerFrameCacheSize;
            metadataCacheSize = visualDebuggerIni().liveConnectionDebuggerMetadataCacheSize;
            return makeDebuggerInternal(isEnabled, path, frameCacheSize, metadataCacheSize);
        }
        case DebuggerType::nxRtpParser:
        {
            isEnabled = visualDebuggerIni().enableNxRtpParserDebuggerInstance;
            path = QString::fromUtf8(visualDebuggerIni().nxRtpParserDebugOutputDirectory);
            frameCacheSize = visualDebuggerIni().nxRtpParserDebuggerFrameCacheSize;
            metadataCacheSize = visualDebuggerIni().nxRtpParserDebuggerMetadataCacheSize;
            return makeDebuggerInternal(isEnabled, path, frameCacheSize, metadataCacheSize);
        }
        default:
        {
            NX_ASSERT(false, lit("We should never be here"));
            return std::make_unique<FakeVisualMetadataDebugger>();
        }
    }
}

VisualMetadataDebuggerPtr VisualMetadataDebuggerFactory::makeDebuggerInternal(
    bool isEnabled,
    const QString& path,
    int frameCacheSize,
    int metadataCacheSize)
{
    if (isEnabled && QDir(path).exists())
    {
        return std::make_unique<VisualMetadataDebugger>(
            path,
            frameCacheSize,
            metadataCacheSize);
    }

    return std::make_unique<FakeVisualMetadataDebugger>();
}

} // namespace debugging
} // namespace nx

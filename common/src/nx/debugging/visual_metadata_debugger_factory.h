#pragma once

#include <nx/debugging/abstract_visual_metadata_debugger.h>

namespace nx {
namespace debugging {

enum class DebuggerType
{
    analyticsManager,
    archiveStreamReader,
    liveConnection,
    nxRtpParser,
};

class VisualMetadataDebuggerFactory
{
public:
    static VisualMetadataDebuggerPtr makeDebugger(
        DebuggerType debuggerType);

private:
    static VisualMetadataDebuggerPtr makeDebuggerInternal(
        bool isEnabled,
        const QString& path,
        int frameCacheSize,
        int metadataCacheSize);
};

} // namespace debugging
} // namespace nx

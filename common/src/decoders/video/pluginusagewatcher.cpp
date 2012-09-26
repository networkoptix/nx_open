////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "pluginusagewatcher.h"


PluginUsageWatcher::PluginUsageWatcher( const QByteArray& uniquePluginID )
{
}

//!Returns list of current decode sessions, using this plugin
QList<DecoderStreamDescription> PluginUsageWatcher::currentSessions() const
{
    //TODO/IMPL
    return QList<DecoderStreamDescription>();
}

void PluginUsageWatcher::decoderCreated( QnAbstractVideoDecoder* const decoder )
{
    //TODO/IMPL
}

void PluginUsageWatcher::decoderIsAboutToBeDestroyed( QnAbstractVideoDecoder* const decoder )
{
    //TODO/IMPL
}

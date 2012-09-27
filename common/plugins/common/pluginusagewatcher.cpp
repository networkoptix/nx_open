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
    //TODO/IMPL checking whether local data is actual
        //if not, locking shared memory
        //reading data from shared memory

    //how to check that data is shared memory is actual?
        //e.g. process has been terminated and it didn't have a chance to clean after itself
        //process id cannot be relied on, since it can be used by newly created process

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

////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "predefinedusagecalculator.h"

#include "pluginusagewatcher.h"


PredefinedUsageCalculator::PredefinedUsageCalculator( PluginUsageWatcher* const usageWatcher )
:
    m_usageWatcher( usageWatcher )
{
}

//!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
bool PredefinedUsageCalculator::isEnoughHWResourcesForAnotherDecoder( const QList<MediaStreamParameter>& mediaStreamParams ) const
{
    //TODO/IMPL
    return true;
}

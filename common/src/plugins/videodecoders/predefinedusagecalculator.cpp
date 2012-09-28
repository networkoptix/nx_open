////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "predefinedusagecalculator.h"

#include "pluginusagewatcher.h"


PredefinedUsageCalculator::PredefinedUsageCalculator( const QString& predefinedDataFilePath, PluginUsageWatcher* const usageWatcher )
:
    m_predefinedDataFilePath( predefinedDataFilePath ),
    m_usageWatcher( usageWatcher ),
    m_currentTree( NULL )
{
    updateTree();
}

//!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
bool PredefinedUsageCalculator::isEnoughHWResourcesForAnotherDecoder( const stree::AbstractResourceReader& mediaStreamParams ) const
{
    //TODO/IMPL retrieving existing sessions' info
    const stree::ResourceContainer& curUsageParams = m_usageWatcher->currentTotalUsage();

    stree::ResourceContainer inputParams;
    //TODO/IMPL summarizing current sessions' parameters and new session parameters

    //TODO/IMPL searching output
    stree::ResourceContainer rc;
    {
        QMutexLocker lk( &m_treeMutex );
        if( m_currentTree )
            m_currentTree->get( inputParams, &rc );
    }

    //analyzing output
    QVariant maxPixelsPerSecond;
    QVariant currentPixelsPerSecond;
    if( rc.get( DecoderParameter::pixelsPerSecond, &maxPixelsPerSecond ) &&
        inputParams.get( DecoderParameter::pixelsPerSecond, &currentPixelsPerSecond ) )
    {
        if( currentPixelsPerSecond.toLongLong() >= maxPixelsPerSecond.toLongLong() )
            return false;
    }
    QVariant maxVideoMemoryUsage;
    QVariant currentVideoMemoryUsage;
    if( rc.get( DecoderParameter::videoMemoryUsage, &maxVideoMemoryUsage ) &&
        inputParams.get( DecoderParameter::videoMemoryUsage, &currentVideoMemoryUsage ) )
    {
        if( currentVideoMemoryUsage.toLongLong() >= maxVideoMemoryUsage.toLongLong() )
            return false;
    }

    return true;
}

void PredefinedUsageCalculator::updateTree()
{
    stree::AbstractNode* newTree = NULL;
    loadXml( QString(), &newTree );
    if( !newTree )
        return ;

    stree::AbstractNode* oldTree = m_currentTree;
    {
        QMutexLocker lk( &m_treeMutex );
        m_currentTree = newTree;
    }
    delete oldTree;
}

void PredefinedUsageCalculator::loadXml( const QString& filePath, stree::AbstractNode** const treeRoot )
{
    //TODO/IMPL
}

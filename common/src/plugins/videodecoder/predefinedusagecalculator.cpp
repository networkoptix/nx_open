////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "predefinedusagecalculator.h"

#include <nx/utils/log/log.h>
#include <nx/utils/stree/node.h>
#include <nx/utils/stree/streesaxhandler.h>

#include "pluginusagewatcher.h"


PredefinedUsageCalculator::PredefinedUsageCalculator(
    const nx::utils::stree::ResourceNameSet& rns,
    const QString& predefinedDataFilePath,
    PluginUsageWatcher* const )
:
    m_rns( rns ),
    m_predefinedDataFilePath( predefinedDataFilePath )
    //m_usageWatcher( usageWatcher )
{
    updateTree();
}

//!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
bool PredefinedUsageCalculator::isEnoughHWResourcesForAnotherDecoder(
    const nx::utils::stree::AbstractResourceReader& mediaStreamParams,
    const nx::utils::stree::AbstractResourceReader& curUsageParams ) const
{
    //retrieving existing sessions' info
    //summarizing current sessions' parameters and new session parameters
    MediaStreamParameterSumContainer inputParams(
        m_rns,
        curUsageParams,
        mediaStreamParams );
    nx::utils::stree::ResourceContainer rc;
    {
        QnMutexLocker lk( &m_treeMutex );
        if( m_currentTree.get() )
            m_currentTree->get( inputParams, &rc );
        else
            return false;
    }

    //analyzing output
    qlonglong maxPixelsPerSecond = 0;
    qlonglong currentPixelsPerSecond = 0;
    if( rc.get( DecoderParameter::pixelsPerSecond, &maxPixelsPerSecond ) &&
        inputParams.get( DecoderParameter::pixelsPerSecond, &currentPixelsPerSecond ) )
    {
        if( currentPixelsPerSecond >= maxPixelsPerSecond )
            return false;
    }

    qlonglong maxVideoMemoryUsage = 0;
    qlonglong currentVideoMemoryUsage = 0;
    if( rc.get( DecoderParameter::videoMemoryUsage, &maxVideoMemoryUsage ) &&
        inputParams.get( DecoderParameter::videoMemoryUsage, &currentVideoMemoryUsage ) )
    {
        if( currentVideoMemoryUsage >= maxVideoMemoryUsage )
            return false;
    }

    int maxSimultaneousStreamCount = 0;
    int currentSimultaneousStreamCount = 0;
    if( rc.get( DecoderParameter::simultaneousStreamCount, &maxSimultaneousStreamCount ) &&
        inputParams.get( DecoderParameter::simultaneousStreamCount, &currentSimultaneousStreamCount ) )
    {
        if( currentSimultaneousStreamCount >= maxSimultaneousStreamCount )
            return false;
    }

    return true;
}

void PredefinedUsageCalculator::updateTree()
{
    if (auto newTree = loadXml(m_predefinedDataFilePath))
    {
        QnMutexLocker lk(&m_treeMutex);
        m_currentTree = std::move(newTree);
    }
}

std::unique_ptr<nx::utils::stree::AbstractNode> PredefinedUsageCalculator::loadXml(
    const QString& filePath)
{
    nx::utils::stree::SaxHandler xmlHandler( m_rns );

    QXmlSimpleReader reader;
    reader.setContentHandler( &xmlHandler );
    reader.setErrorHandler( &xmlHandler );

    QFile xmlFile( filePath );
    if( !xmlFile.open( QIODevice::ReadOnly ) )
    {
        NX_LOG( lit( "Failed to open stree xml file (%1). %2" ).arg(filePath).arg(xmlFile.errorString()), cl_logERROR );
        return std::unique_ptr<nx::utils::stree::AbstractNode>();
    }
    QXmlInputSource input( &xmlFile );
    NX_LOG( lit( "Parsing stree xml file (%1)" ).arg(filePath), cl_logDEBUG1 );
    if( !reader.parse( &input ) )
    {
        NX_LOG( lit( "Failed to parse stree xml (%1). %2" ).arg(filePath).arg(xmlHandler.errorString()), cl_logERROR );
        return std::unique_ptr<nx::utils::stree::AbstractNode>();
    }

    return xmlHandler.releaseTree();
}

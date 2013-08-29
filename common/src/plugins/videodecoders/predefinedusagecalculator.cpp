////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "predefinedusagecalculator.h"

#include "pluginusagewatcher.h"
#include "stree/streesaxhandler.h"


PredefinedUsageCalculator::PredefinedUsageCalculator(
    const stree::ResourceNameSet& rns,
    const QString& predefinedDataFilePath,
    PluginUsageWatcher* const usageWatcher )
:
    m_rns( rns ),
    m_predefinedDataFilePath( predefinedDataFilePath ),
    m_usageWatcher( usageWatcher ),
    m_currentTree( NULL )
{
    updateTree();
}

//!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
bool PredefinedUsageCalculator::isEnoughHWResourcesForAnotherDecoder(
    const stree::AbstractResourceReader& mediaStreamParams,
    const stree::AbstractResourceReader& curUsageParams ) const
{
    //retrieving existing sessions' info
    //summarizing current sessions' parameters and new session parameters
    MediaStreamParameterSumContainer inputParams(
        m_rns,
        curUsageParams,
        mediaStreamParams );
    stree::ResourceContainer rc;
    {
        QMutexLocker lk( &m_treeMutex );
        if( m_currentTree.get() )
            m_currentTree->get( inputParams, &rc );
        else
            return false;
    }

    //analyzing output
    qlonglong maxPixelsPerSecond = 0;
    qlonglong currentPixelsPerSecond = 0;
    if( rc.getTypedVal( DecoderParameter::pixelsPerSecond, &maxPixelsPerSecond ) &&
        inputParams.getTypedVal( DecoderParameter::pixelsPerSecond, &currentPixelsPerSecond ) )
    {
        if( currentPixelsPerSecond >= maxPixelsPerSecond )
            return false;
    }

    qlonglong maxVideoMemoryUsage = 0;
    qlonglong currentVideoMemoryUsage = 0;
    if( rc.getTypedVal( DecoderParameter::videoMemoryUsage, &maxVideoMemoryUsage ) &&
        inputParams.getTypedVal( DecoderParameter::videoMemoryUsage, &currentVideoMemoryUsage ) )
    {
        if( currentVideoMemoryUsage >= maxVideoMemoryUsage )
            return false;
    }

    int maxSimultaneousStreamCount = 0;
    int currentSimultaneousStreamCount = 0;
    if( rc.getTypedVal( DecoderParameter::simultaneousStreamCount, &maxSimultaneousStreamCount ) &&
        inputParams.getTypedVal( DecoderParameter::simultaneousStreamCount, &currentSimultaneousStreamCount ) )
    {
        if( currentSimultaneousStreamCount >= maxSimultaneousStreamCount )
            return false;
    }

    return true;
}

void PredefinedUsageCalculator::updateTree()
{
    stree::AbstractNode* newTree = NULL;
    loadXml( m_predefinedDataFilePath, &newTree );
    if( !newTree )
        return;

    std::auto_ptr<stree::AbstractNode> oldTree;
    {
        QMutexLocker lk( &m_treeMutex );
        oldTree = m_currentTree;
        m_currentTree.reset( newTree );
    }
}

void PredefinedUsageCalculator::loadXml( const QString& filePath, stree::AbstractNode** const treeRoot )
{
    stree::SaxHandler xmlHandler( m_rns );

    QXmlSimpleReader reader;
    reader.setContentHandler( &xmlHandler );
    reader.setErrorHandler( &xmlHandler );

    QFile xmlFile( filePath );
    if( !xmlFile.open( QIODevice::ReadOnly ) )
    {
        cl_log.log( QString::fromLatin1( "Failed to open stree xml file (%1). %2" ).arg(filePath).arg(xmlFile.errorString()), cl_logERROR );
        return;
    }
    QXmlInputSource input( &xmlFile );
    cl_log.log( QString::fromLatin1( "Parsing stree xml file (%1)" ).arg(filePath), cl_logDEBUG1 );
    if( !reader.parse( &input ) )
    {
        cl_log.log( QString::fromLatin1( "Failed to parse stree xml (%1). %2" ).arg(filePath).arg(xmlHandler.errorString()), cl_logERROR );
        return;
    }

    *treeRoot = xmlHandler.releaseTree();
}

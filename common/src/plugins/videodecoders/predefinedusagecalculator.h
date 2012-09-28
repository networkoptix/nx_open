////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PREDEFINEDUSAGECALCULATOR_H
#define PREDEFINEDUSAGECALCULATOR_H

#include <QMutex>
#include <QString>

#include "abstractvideodecoderusagecalculator.h"
#include "videodecoderplugintypes.h"
#include "stree/node.h"


class PluginUsageWatcher;

//!Estimates resources availability based on current load and xml file with predefined values
class PredefinedUsageCalculator
:
    public AbstractVideoDecoderUsageCalculator
{
public:
    /*!
        \param usageWatcher
    */
    PredefinedUsageCalculator( const QString& predefinedDataFilePath, PluginUsageWatcher* const usageWatcher );
    
    //!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
    virtual bool isEnoughHWResourcesForAnotherDecoder( const stree::AbstractResourceReader& mediaStreamParams ) const;

private:
    const QString m_predefinedDataFilePath;
    PluginUsageWatcher* const m_usageWatcher;
    stree::AbstractNode* m_currentTree;
    mutable QMutex m_treeMutex;

    void updateTree();
    void loadXml( const QString& filePath, stree::AbstractNode** const treeRoot );
};

#endif  //PREDEFINEDUSAGECALCULATOR_H

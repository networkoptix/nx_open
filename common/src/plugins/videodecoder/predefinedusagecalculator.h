////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PREDEFINEDUSAGECALCULATOR_H
#define PREDEFINEDUSAGECALCULATOR_H

#include <memory>

#include <nx/tool/thread/mutex.h>
#include <QtCore/QString>

#include "stree/resourcenameset.h"

#include "abstractvideodecoderusagecalculator.h"
#include "videodecoderplugintypes.h"

namespace stree {
    class AbstractNode;
};

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
    PredefinedUsageCalculator(
        const stree::ResourceNameSet& rns,
        const QString& predefinedDataFilePath,
        PluginUsageWatcher* const usageWatcher );
    
    //!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
    virtual bool isEnoughHWResourcesForAnotherDecoder(
        const stree::AbstractResourceReader& mediaStreamParams,
        const stree::AbstractResourceReader& curUsageParams ) const override;

private:
    const stree::ResourceNameSet& m_rns;
    const QString m_predefinedDataFilePath;
    //PluginUsageWatcher* const m_usageWatcher;
    std::unique_ptr<stree::AbstractNode> m_currentTree;
    mutable QnMutex m_treeMutex;

    void updateTree();
    std::unique_ptr<stree::AbstractNode> loadXml(const QString& filePath);
};

#endif  //PREDEFINEDUSAGECALCULATOR_H

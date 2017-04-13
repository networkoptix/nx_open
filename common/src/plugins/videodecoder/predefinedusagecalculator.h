////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PREDEFINEDUSAGECALCULATOR_H
#define PREDEFINEDUSAGECALCULATOR_H

#include <memory>

#include <QtCore/QString>

#include <nx/utils/stree/resourcenameset.h>
#include <nx/utils/thread/mutex.h>

#include "abstractvideodecoderusagecalculator.h"
#include "videodecoderplugintypes.h"

namespace nx {
namespace utils {
namespace stree {

class AbstractNode;

} // namespace stree
} // namespace utils
} // namespace nx

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
        const nx::utils::stree::ResourceNameSet& rns,
        const QString& predefinedDataFilePath,
        PluginUsageWatcher* const usageWatcher );
    
    //!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
    virtual bool isEnoughHWResourcesForAnotherDecoder(
        const nx::utils::stree::AbstractResourceReader& mediaStreamParams,
        const nx::utils::stree::AbstractResourceReader& curUsageParams ) const override;

private:
    const nx::utils::stree::ResourceNameSet& m_rns;
    const QString m_predefinedDataFilePath;
    //PluginUsageWatcher* const m_usageWatcher;
    std::unique_ptr<nx::utils::stree::AbstractNode> m_currentTree;
    mutable QnMutex m_treeMutex;

    void updateTree();
    std::unique_ptr<nx::utils::stree::AbstractNode> loadXml(const QString& filePath);
};

#endif  //PREDEFINEDUSAGECALCULATOR_H

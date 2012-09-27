////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PREDEFINEDUSAGECALCULATOR_H
#define PREDEFINEDUSAGECALCULATOR_H

#include "abstractvideodecoderusagecalculator.h"

#include "videodecoderplugintypes.h"


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
    PredefinedUsageCalculator( PluginUsageWatcher* const usageWatcher );
    
    //!Implementation of AbstractVideoDecoderUsageCalculator::isEnoughHWResourcesForAnotherDecoder
    virtual bool isEnoughHWResourcesForAnotherDecoder( const QList<MediaStreamParameter>& mediaStreamParams ) const;

private:
    PluginUsageWatcher* const m_usageWatcher;
};

#endif  //PREDEFINEDUSAGECALCULATOR_H

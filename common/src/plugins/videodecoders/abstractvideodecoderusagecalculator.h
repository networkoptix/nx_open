////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTVIDEODECODERUSAGECALCULATOR_H
#define ABSTRACTVIDEODECODERUSAGECALCULATOR_H

#include "videodecoderplugintypes.h"


//!Calculates whether new decoder instance will do fine
class AbstractVideoDecoderUsageCalculator
{
public:
    //!Returns true, if there are free resources (defined in \a curUsageParams) to decode media stream with parameters \a mediaStreamParams
    virtual bool isEnoughHWResourcesForAnotherDecoder(
        const stree::AbstractResourceReader& mediaStreamParams,
        const stree::AbstractResourceReader& curUsageParams ) const = 0;
};

#endif  //ABSTRACTVIDEODECODERUSAGECALCULATOR_H

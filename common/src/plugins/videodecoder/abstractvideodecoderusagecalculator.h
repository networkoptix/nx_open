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
    //!Returns true, if there are free resources to decode media stream with parameters \a mediaStreamParams
    /*!
        \param mediaStreamParams Params of new media stream
        \param curUsageParams Currently used resources are taken from usage watcher
    */
    virtual bool isEnoughHWResourcesForAnotherDecoder(
        const stree::AbstractResourceReader& mediaStreamParams,
        const stree::AbstractResourceReader& curUsageParams ) const = 0;
};

#endif  //ABSTRACTVIDEODECODERUSAGECALCULATOR_H

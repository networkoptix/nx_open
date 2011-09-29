#ifndef ABSTRACTRENDERER_H
#define ABSTRACTRENDERER_H

#include "decoders/videodata.h"

class CLAbstractRenderer
{
public:
    virtual void draw(CLVideoDecoderOutput &image, unsigned int channel) = 0;
    virtual void beforeDestroy() = 0;

    virtual QSize sizeOnScreen(unsigned int channel) const = 0;

    virtual bool constantDownscaleFactor() const = 0;
};

#endif // ABSTRACTRENDERER_H

#ifndef ABSTRACTRENDERER_H
#define ABSTRACTRENDERER_H

#include "decoders/videodata.h"

class CLAbstractRenderer
{
public:
    CLAbstractRenderer()
        : m_copyData(false)
    {
    }

    virtual void draw(CLVideoDecoderOutput &image, unsigned int channel) = 0;
    virtual void beforeDestroy() = 0;

    virtual QSize sizeOnScreen(unsigned int channel) const = 0;

    virtual bool constantDownscaleFactor() const = 0;

    virtual void copyVideoDataBeforePainting(bool copyData)
    {
        m_copyData = copyData;
    }

protected:
    bool m_copyData;
};

#endif // ABSTRACTRENDERER_H

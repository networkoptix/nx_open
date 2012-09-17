#ifndef QN_ABSTRACT_RENDERER_H
#define QN_ABSTRACT_RENDERER_H

#include "utils/media/frame_info.h"

class CLVideoDecoderOutput;

/**
 * This class is supposed to be used from two threads &mdash; a <i>rendering</i> thread
 * and a <i>decoding</i> thread.
 * 
 * Decoding thread prepares the frames to be displayed, and rendering thread displays them.
 * 
 * Note that it is owned by the rendering thread.
 */
class QnAbstractRenderer
{
public:
    QnAbstractRenderer(): m_displayCounter(0) {}

    virtual ~QnAbstractRenderer() {}

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It waits until given channel of the current frame (supplied via <tt>draw</tt>) is rendered.
     * 
     * \param channel                   Channel number.
     */
    virtual void waitForFrameDisplayed(int channel) = 0;

    /**
     * This function is supposed to be called from <i>rendering</i> thread.
     * It notifies the <i>decoding</i> thread that the current frame was rendered.
     */
    void frameDisplayed() {
        m_displayCounter++;

        doFrameDisplayed();
    }

    /**
     * This function may be called from any thread.
     * It is called just before this object is destroyed.
     */
    virtual void beforeDestroy() = 0;

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * 
     * \param channel                   Channel number.
     * \returns                         Size of the given channel on rendering device.
     */
    virtual QSize sizeOnScreen(unsigned int channel) const = 0;

    /**
     * This function is supposed to be called from <i>decoding</t> thread.
     * 
     * \returns                         Whether the downscale factor is forced x2 constant.
     */
    virtual bool constantDownscaleFactor() const = 0;

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It notifies the derived class that a new frame is available for display.
     * It is up to derived class to supply this new frame to the <i>rendering</i> thread.
     * 
     * \param image                     New video frame.
     */
    virtual void draw(CLVideoDecoderOutput *image) = 0;

    /**
     * \returns                         Value of this renderer's display counter.
     *                                  This counter is incremented each time a frame
     *                                  is rendered.
     */
    int displayCounter() const {
        return m_displayCounter;
    }

    /**
     * Inform drawer about video is temporary absent
     */
    virtual void onNoVideo() {}

    /**
     * Returns last displayed time
     */
    virtual qint64 lastDisplayedTime() const { return AV_NOPTS_VALUE; }


protected:
    virtual void doFrameDisplayed() {} // Not used for now.


private:
    int m_displayCounter;
};

#endif // QN_ABSTRACT_RENDERER_H

#ifndef QN_ABSTRACT_RENDERER_H
#define QN_ABSTRACT_RENDERER_H

#include "utils/media/frame_info.h"
#include "utils/common/stoppable.h"


class CLVideoDecoderOutput;

/**
 * This class is supposed to be used from two threads &mdash; a <i>rendering</i> thread
 * and a <i>decoding</i> thread.
 * 
 * Decoding thread prepares the frames to be displayed, and rendering thread displays them.
 * 
 * Note that it is owned by the rendering thread.
 */
class QnAbstractRenderer: public QObject, public QnStoppable
{
    Q_OBJECT
signals:
    void canBeDestroyed();
public:
    QnAbstractRenderer(QObject* parent = 0): QObject(parent), m_displayCounter(0), m_useCount(0), m_needStop(false) {}

    virtual ~QnAbstractRenderer() {}

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It waits until given channel of the current frame (supplied via <tt>draw</tt>) is rendered.
     * 
     * \param channel                   Channel number.
     */
    virtual void waitForFrameDisplayed(int channel) = 0;

    /**
     * Upon return there is no frames to display and renderer has finished displaying current frame
    */
    virtual void discardAllFramesPostedToDisplay(int channel) = 0;

    /**
     * Blocks until all frames passed to \a draw are surely displayed on screen.
     * Difference from \a waitForFrameDisplayed is that waitForFrameDisplayed may not wait for frames displayed, but only ensure, 
     * they will be displayed sometimes. This is required to take advantage of async frame uploading and for effective usage of hardware decoder: 
     * it should spend as much time as possible in \a decode method, but not waiting for frame to be rendered.
     *
     * \todo refactoring (some renaming?) is required when it all works as expected
     */
    virtual void finishPostedFramesRender(int channel) = 0;

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
    virtual void destroyAsync() 
    {
        QMutexLocker lock(&m_usingMutex);
        m_needStop = true;
        if (m_useCount == 0)
            emit canBeDestroyed();
    }

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
    virtual void draw( const QSharedPointer<CLVideoDecoderOutput>& image) = 0;

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

    //!Returns timestamp of frame that will be rendered next. It can be already displayed frame (if no new frames available)
    virtual qint64 getTimestampOfNextFrameToRender(int channelNumber) const  = 0;
    virtual void blockTimeValue(int channelNumber, qint64  timestamp ) const = 0;
    virtual void unblockTimeValue(int channelNumber) const = 0;
    virtual bool isTimeBlocked(int channelNumber) const  = 0;

    virtual bool isDisplaying( const QSharedPointer<CLVideoDecoderOutput>& image ) const = 0;

    void inUse() {
        QMutexLocker lock(&m_usingMutex);
        m_useCount++; 
    }
    void notInUse() { 
        QMutexLocker lock(&m_usingMutex);
        if (--m_useCount == 0 && m_needStop)
            emit canBeDestroyed();
    }
protected:
    virtual void doFrameDisplayed() {} // Not used for now.


private:
    int m_displayCounter;
    int m_useCount;
    bool m_needStop;
    QMutex m_usingMutex;
};

#endif // QN_ABSTRACT_RENDERER_H

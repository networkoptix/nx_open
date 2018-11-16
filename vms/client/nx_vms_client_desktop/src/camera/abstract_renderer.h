#pragma once

#include <chrono>

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/mutex.h>

#include <utils/media/frame_info.h>

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

public:
    QnAbstractRenderer(QObject* parent = nullptr): QObject(parent) {}

    virtual ~QnAbstractRenderer() override {}

    /**
     * This function is supposed to be called from <i>decoding</i> thread.
     * It waits until given channel of the current frame (supplied via <tt>draw</tt>) is rendered.
     *
     * \param channel                   Channel number.
     */
    virtual void waitForFrameDisplayed(int channel) = 0;

    virtual void waitForQueueLessThan(int channel, int count) = 0;

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
     * This function may be called from any thread.
     * It is called just before this object is destroyed.
     */
    virtual void destroyAsync()
    {
        QnMutexLocker lock(&m_usingMutex);
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
    virtual QSize sizeOnScreen(int channel) const = 0;

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
    virtual void draw(const QSharedPointer<CLVideoDecoderOutput>& image) = 0;

    /**
     * Inform drawer about video is temporary absent
     */
    virtual void onNoVideo() {}

    //!Returns timestamp of frame that will be rendered next. It can be already displayed frame (if no new frames available)
    virtual std::chrono::microseconds getTimestampOfNextFrameToRender(int channelNumber) const = 0;
    virtual void blockTimeValue(int channelNumber, std::chrono::microseconds timestamp) const = 0;
    virtual void unblockTimeValue(int channelNumber) const = 0;
    virtual bool isTimeBlocked(int channelNumber) const = 0;
    virtual bool isDisplaying(const QSharedPointer<CLVideoDecoderOutput>& image) const = 0;

    void inUse()
    {
        QnMutexLocker lock(&m_usingMutex);
        NX_ASSERT(!m_needStop);
        m_useCount++;
    }

    void notInUse()
    {
        QnMutexLocker lock(&m_usingMutex);
        if (--m_useCount == 0 && m_needStop)
            emit canBeDestroyed();
    }

    virtual bool isEnabled(int channelNumber) const = 0;
    /*!
        Enable/disable frame rendering for channel \a channelNumber. true by default.
        Disabling causes all previously posted frames being discarded. Any subsequent frames will be ignored
    */
    virtual void setEnabled(int channelNumber, bool enabled) = 0;

    /*!
        Inform render that media stream is paused and no more frames expected
    */
    virtual void setPaused(bool value) = 0;

    virtual void setScreenshotInterface(ScreenshotInterface* value) = 0;

signals:
    void canBeDestroyed();

private:
    int m_useCount = 0;
    bool m_needStop = false;
    QnMutex m_usingMutex;
};

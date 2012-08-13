#ifndef QnExternalTimeSource_h_1919
#define QnExternalTimeSource_h_1919

#include <QtGlobal> /* For Q_UNUSED. */

class QnlTimeSource
{
public:
    /*
    * @return current time. May be different from displayed time. After seek for example, while no any frames are really displayed
    */
    virtual qint64 getCurrentTime() const = 0;

    /*
    * @return Return last displayed time
    */
    virtual qint64 getDisplayedTime() const = 0;

    /*
    * @return Return time of the next frame
    */
    virtual qint64 getNextTime() const = 0;

    /*
    * @return time of the external time source. Syncplay time of camDisplay for example
    */
    virtual qint64 getExternalTime() const = 0;

    /*
    * @return expected time. For example, time based on local timer
    */
    virtual qint64 expectedTime() const { return getCurrentTime(); }


    virtual void onBufferingStarted(QnlTimeSource* src, qint64 firstTime) { Q_UNUSED(src); Q_UNUSED(firstTime);}
    virtual void onBufferingFinished(QnlTimeSource* src) { Q_UNUSED(src); }
    virtual void onEofReached(QnlTimeSource* src, bool value) { Q_UNUSED(src); Q_UNUSED(value); }
    virtual bool isEnabled() const { return true; }

    virtual void reinitTime(qint64 newTime) { Q_UNUSED(newTime); }
    
    /*
    * Set time modificators for time source
    * @param timeOffsetUsec relative time offset of the time source at usec
    * @param minTimeUsec additional time range bounds
    * @param maxTimeUsec additional time range bounds
    */
    virtual void setTimeParams(qint64 timeOffsetUsec, qint64 minTimeUsec, qint64 maxTimeUsec) { Q_UNUSED(timeOffsetUsec); Q_UNUSED(minTimeUsec); Q_UNUSED(maxTimeUsec) }
};

#endif //QnExternalTimeSource_h_1919

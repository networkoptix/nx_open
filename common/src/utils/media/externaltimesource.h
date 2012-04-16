#ifndef QnExternalTimeSource_h_1919
#define QnExternalTimeSource_h_1919

#include <QtGlobal> /* For Q_UNUSED. */

class QnlTimeSource
{
public:
    virtual qint64 getCurrentTime() const = 0;
    virtual qint64 getDisplayedTime() const = 0;
    virtual qint64 getNextTime() const = 0;
    virtual qint64 getExternalTime() const = 0;

    virtual qint64 expectedTime() const { return getCurrentTime(); }


    virtual void onBufferingStarted(QnlTimeSource* src, qint64 firstTime) { Q_UNUSED(src); Q_UNUSED(firstTime);}
    virtual void onBufferingFinished(QnlTimeSource* src) { Q_UNUSED(src); }
    virtual void onEofReached(QnlTimeSource* src, bool value) { Q_UNUSED(src); Q_UNUSED(value); }
    virtual bool isEnabled() const { return true; }

    virtual void reinitTime(qint64 newTime) {}
    //virtual void onAvailableTime(QnlTimeSource* src, qint64 time) {}
};

#endif //QnExternalTimeSource_h_1919

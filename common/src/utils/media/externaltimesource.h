#ifndef QnExternalTimeSource_h_1919
#define QnExternalTimeSource_h_1919

#include <QtGlobal> /* For Q_UNUSED. */

class QnlTimeSource
{
public:
    virtual qint64 getCurrentTime() const = 0;
    virtual qint64 getDisplayedTime() const = 0;
    virtual qint64 getNextTime() const = 0;

    virtual void onBufferingStarted(QnlTimeSource* src) { Q_UNUSED(src); }
    virtual void onBufferingFinished(QnlTimeSource* src) { Q_UNUSED(src); }
    //virtual void onAvailableTime(QnlTimeSource* src, qint64 time) {}
};

#endif //QnExternalTimeSource_h_1919

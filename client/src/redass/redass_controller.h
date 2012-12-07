#ifndef __QN_REDASS_CONTROLLER_H__
#define __QN_REDASS_CONTROLLER_H__

#include "core/datapacket/media_data_packet.h"
#include "utils/common/synctime.h"

class QnCamDisplay;
class QnArchiveStreamReader;

class QnRedAssController: public QThread
{
    Q_OBJECT
public:
    QnRedAssController();

    static QnRedAssController* instance();

    void registerConsumer(QnCamDisplay* display);
    void unregisterConsumer(QnCamDisplay* display);
    int counsumerCount() const;

    bool isPrecSeekAllowed(QnCamDisplay* currentDisplay);
public slots:
    /* Inform controller that not enough data or CPU for stream */
    void onSlowStream(QnArchiveStreamReader* reader);

    /* Inform controller that no more problem with stream */
    void streamBackToNormal(QnArchiveStreamReader* reader);
private slots:
    void onTimer();
private:
    QnCamDisplay* getDisplayByReader(QnArchiveStreamReader* reader);
    bool isSmallItem(QnCamDisplay* display);
    bool isNotSmallItem(QnCamDisplay* display);
    bool isSupportedDisplay(QnCamDisplay* display) const;

    /** try LQ->HQ once more */
    void addHQTry(); 

private:
    enum FindMethod {Find_Biggest, Find_Least};
    enum LQReason {Reason_None, Reason_Small, Reason_Network, Reason_CPU, Reson_FF};
    struct RedAssInfo
    {
        RedAssInfo(): lqTime(0), toLQSpeed(0.0), lqReason(Reason_None), initialTime(qnSyncTime->currentMSecsSinceEpoch()) {}
        qint64 lqTime;
        float toLQSpeed;
        LQReason lqReason;
        qint64 initialTime;
    };

    typedef bool (QnRedAssController::*SearchCondition)(QnCamDisplay*);

    mutable QMutex m_mutex;
    typedef QMap<QnCamDisplay*, RedAssInfo> ConsumersMap;
    ConsumersMap m_redAssInfo;
    QTimer m_timer;
    QTime m_lastSwitchTimer; // latest HQ->LQ or LQ->HQ switch
    int m_hiQualityRetryCounter;
    int m_timerTicks;    // onTimer ticks count
    qint64 m_lastLqTime; // latest HQ->LQ switch time
private:
    QnCamDisplay* findDisplay(FindMethod method, MediaQuality findQuality, SearchCondition cond = 0, int* displaySize = 0);
    void gotoLowQuality(QnCamDisplay* display, LQReason reason);
    void optimizeItemsQualityBySize();
};

#define qnRedAssController QnRedAssController::instance()

#endif // __QN_REDASS_CONTROLLER_H__

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

    /** Inform that not enought CPU or badnwidth */
    //void setQuality(QnCamDisplay* display, MediaQuality quality);
public slots:
    void onSlowStream(QnArchiveStreamReader* reader);
    void streamBackToNormal(QnArchiveStreamReader* reader);
private slots:
    void onTimer();
private:
    QnCamDisplay* getDisplayByReader(QnArchiveStreamReader* reader);
    bool isSmallItem(QnCamDisplay* display);
    bool isNotSmallItem(QnCamDisplay* display);

    /** try LQ->HQ once more */
    void addHQTry(); 

private:
    enum FindMethod {Find_Biggest, Find_Least};
    enum LQReason {Reason_None, Reason_Small, Reason_Network, Reason_CPU, Reson_FF};
    struct RedAssInfo
    {
        RedAssInfo(): lqTime(0), /* hiQualityRetryCounter(0), */ toLQSpeed(0.0), lqReason(Reason_None), initialTime(qnSyncTime->currentMSecsSinceEpoch()) {}
        qint64 lqTime;
        //int hiQualityRetryCounter;
        float toLQSpeed;
        LQReason lqReason;
        qint64 initialTime;
    };

    typedef bool (QnRedAssController::*SearchCondition)(QnCamDisplay*);

    QMutex m_mutex;
    typedef QMap<QnCamDisplay*, RedAssInfo> ConsumersMap;
    ConsumersMap m_redAssInfo;
    QTimer m_timer;
    QTime m_lastSwitchTimer;
    int m_hiQualityRetryCounter;
    int m_timerTicks;
private:
    QnCamDisplay* findDisplay(FindMethod method, bool findHQ, SearchCondition cond = 0, int* displaySize = 0);
    void gotoLowQuality(QnCamDisplay* display, LQReason reason);
    void optimizeItemsQualityBySize();
};

#define qnRedAssController QnRedAssController::instance()

#endif // __QN_REDASS_CONTROLLER_H__

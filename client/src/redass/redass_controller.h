#ifndef __QN_REDASS_CONTROLLER_H__
#define __QN_REDASS_CONTROLLER_H__

#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QThread>

#include "core/datapacket/media_data_packet.h"
#include "utils/common/synctime.h"

#include <client/client_globals.h>

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

    void setMode(Qn::ResolutionMode mode);
    Qn::ResolutionMode getMode() const;

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
    bool isSmallItem2(QnCamDisplay* display);
    bool itemCanBeOptimized(QnCamDisplay* display);
    bool isNotSmallItem2(QnCamDisplay* display);

    bool isSupportedDisplay(QnCamDisplay* display) const;
    bool isForcedHQDisplay(QnCamDisplay* display, QnArchiveStreamReader* reader) const;

    /** try LQ->HQ once more */
    void addHQTry(); 
    bool isFFSpeed(QnCamDisplay* display) const;
    bool isFFSpeed(double speed) const;
    bool existstBufferingDisplay() const;
private:
    // TODO: #Elric #enum
    enum FindMethod {Find_Biggest, Find_Least};
    enum LQReason {Reason_None, Reason_Small, Reason_Network, Reason_CPU, Reson_FF};
    struct RedAssInfo
    {
        RedAssInfo(): lqTime(0), toLQSpeed(0.0), lqReason(Reason_None), initialTime(qnSyncTime->currentMSecsSinceEpoch()), awaitingLQTime(0), smallSizeCnt(0) {}
        qint64 lqTime;
        float toLQSpeed;
        LQReason lqReason;
        qint64 initialTime;
        qint64 awaitingLQTime;
        int smallSizeCnt;
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
    Qn::ResolutionMode m_mode;
private:
    QnCamDisplay* findDisplay(FindMethod method, MediaQuality findQuality, SearchCondition cond = 0, int* displaySize = 0);
    void gotoLowQuality(QnCamDisplay* display, LQReason reason, double speed = INT_MAX);
    void optimizeItemsQualityBySize();
};

#define qnRedAssController QnRedAssController::instance()

#endif // __QN_REDASS_CONTROLLER_H__

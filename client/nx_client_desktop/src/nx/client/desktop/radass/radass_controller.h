#pragma once

#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QThread>

#include <nx/client/desktop/radass/radass_fwd.h>

#include "nx/streaming/media_data_packet.h"
#include "utils/common/synctime.h"

class QnCamDisplay;
class QnArchiveStreamReader;

namespace nx {
namespace client {
namespace desktop {

class RadassController: public QThread
{
    Q_OBJECT
public:
    RadassController();

    void registerConsumer(QnCamDisplay* display);
    void unregisterConsumer(QnCamDisplay* display);
    int counsumerCount() const;

    void setMode(RadassMode mode);
    RadassMode getMode() const;

public slots:
    /* Inform controller that not enough data or CPU for stream */
    void onSlowStream(QnArchiveStreamReader* reader);

    /* Inform controller that no more problem with stream */
    void streamBackToNormal(QnArchiveStreamReader* reader);

private:
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
    struct ConsumerInfo;

    typedef bool (RadassController::*SearchCondition)(QnCamDisplay*);

    mutable QnMutex m_mutex;
    typedef QMap<QnCamDisplay*, ConsumerInfo> ConsumersMap;
    ConsumersMap m_consumersInfo;
    QTimer m_timer;
    QElapsedTimer m_lastSwitchTimer; //< Latest HQ->LQ or LQ->HQ switch
    int m_hiQualityRetryCounter;
    int m_timerTicks;    // onTimer ticks count
    qint64 m_lastLqTime; // latest HQ->LQ switch time
    RadassMode m_mode;

private:
    QnCamDisplay* findDisplay(FindMethod method, MediaQuality findQuality, SearchCondition cond = 0, int* displaySize = 0);
    void gotoLowQuality(QnCamDisplay* display, LQReason reason, double speed = INT_MAX);
    void optimizeItemsQualityBySize();
};

} // namespace desktop
} // namespace client
} // namespace nx

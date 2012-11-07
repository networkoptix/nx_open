#ifndef __QN_REDASS_CONTROLLER_H__
#define __QN_REDASS_CONTROLLER_H__

#include "core/datapacket/media_data_packet.h"

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
private:
    void onTimer();
    QnCamDisplay* getDisplayByReader(QnArchiveStreamReader* reader);
private:
    struct RedAssInfo
    {
        RedAssInfo(): lqTime(AV_NOPTS_VALUE), hiQualityRetryCounter(0) {}
        qint64 lqTime;
        int hiQualityRetryCounter;
    };

    QMutex m_mutex;
    QSet<QnCamDisplay*> m_consumers;
    QMap<QnCamDisplay*, RedAssInfo> m_redAssInfo;
    QTimer m_timer;
    QTime m_lastSwitchTimer;
};

#define qnRedAssController QnRedAssController::instance()

#endif // __QN_REDASS_CONTROLLER_H__

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
    void setQuality(QnCamDisplay* display, MediaQuality quality);
public slots:
    void onSlowStream(QnArchiveStreamReader* reader);
private:
    void onTimer();
private:
    QMutex m_mutex;
    QSet<QnCamDisplay*> m_consumers;
    QMap<QnCamDisplay*, qint64> m_lowQualityRequests;
    QTimer m_timer;
    QTime m_lastSwitchTimer;
};

#define qnRedAssController QnRedAssController::instance()

#endif // __QN_REDASS_CONTROLLER_H__

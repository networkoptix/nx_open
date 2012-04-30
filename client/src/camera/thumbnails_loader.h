#ifndef __THUMBNAILS_LOADER_H__
#define __THUMBNAILS_LOADER_H__
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"

class QnThumbnailsLoader: public QObject, CLLongRunnable
{
    Q_OBJECT
public:
    QnThumbnailsLoader(QnResourcePtr resource);
    virtual ~QnThumbnailsLoader();

    void setThumbnailsSize(int width, int height);

    /*
    * Load video pixmaps by specified time
    */
    void loadRange(qint64 startTime, qint64 endTime, qint64 step);

    /* Extent existing range to new range using previously defined step */
    void addRange(qint64 startTime, qint64 endTime);

    /* Remove part of data or all data */
    void removeRange(qint64 startTime=-1, qint64 endTime=-1);

    QPixmap getPixmapByTime(qint64 time);
signals:
    void gotNewPixmap(qint64 time, QPixmap image);
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
private:
    QScopedPointer<QnRtspClientArchiveDelegate> m_rtspClient;
    QMap<qint64, QPixmap> m_images;
    QnResourcePtr m_resource;

    qint64 m_step;
    qint64 m_startTime;
    qint64 m_endTime;
    int m_outWidth;
    int m_outHeight;
};

#endif // __THUMBNAILS_LOADER_H__

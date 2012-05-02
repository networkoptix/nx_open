#ifndef __THUMBNAILS_LOADER_H__
#define __THUMBNAILS_LOADER_H__
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"

class QnThumbnailsLoader: CLLongRunnable
{
    Q_OBJECT
public:
    QnThumbnailsLoader(QnResourcePtr resource);
    virtual ~QnThumbnailsLoader();

    void setThumbnailsSize(int width, int height);

    /*
    * Load video pixmaps by specified time
    */
    void loadRange(qint64 startTimeUsec, qint64 endTimeUsec, qint64 stepUsec);

    /* Extent existing range to new range using previously defined step */
    void addRange(qint64 startTimeUsec, qint64 endTimeUsec);

    /* 
    * Remove part of data or all data 
    */
    void removeRange(qint64 startTimeUsec = -1, qint64 endTimeUsec = INT64_MAX);

    /* 
    * Find pixmap by specified time. 
    * @param timeUsec contain approximate time. 
    * @param realPixmapTimeUsec Return exact pixmap time if found. Otherwise return -1
    */
    QPixmap getPixmapByTime(qint64 timeUsec, quint64* realPixmapTimeUsec = 0);

signals:
    void gotNewPixmap(qint64 timeUsec, QPixmap pixmap);
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
private:
    void allocateScaleContext(int linesize, int width, int height, PixelFormat format);
private:
    QScopedPointer<QnRtspClientArchiveDelegate> m_rtspClient;
    QMap<qint64, QPixmap> m_images;
    QnResourcePtr m_resource;

    qint64 m_step;
    qint64 m_startTime;
    qint64 m_endTime;
    int m_outWidth;
    int m_outHeight;
    QQueue<QnTimePeriod> m_rangeToLoad;
    QMutex m_mutex;
    SwsContext* m_scaleContext;
    quint8* m_rgbaBuffer;
    int m_srcLineSize;
    int m_srcWidth;
    int m_srcHeight;
    int m_srcFormat;
};

#endif // __THUMBNAILS_LOADER_H__

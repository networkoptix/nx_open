#ifndef __THUMBNAILS_LOADER_H__
#define __THUMBNAILS_LOADER_H__
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "utils/media/frame_info.h"

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
    void loadRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs);

    QnTimePeriod loadedRange() const;
    qint64 lastLoadingTime() const;

    /*
    * thumbnails step in ms
    */
    qint64 step() const;


    /* Extent existing range to new range using previously defined step */
    void addRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs);

    /* 
    * Remove part of data or all data 
    */
    void removeRange(qint64 startTimeMs = -1, qint64 endTimeMs = INT64_MAX);

    /* 
    * Find pixmap by specified time. 
    * @param timeMs contain approximate time. 
    * @param realPixmapTimeMs Return exact pixmap time if found. Otherwise return -1
    */
    const QPixmap *getPixmapByTime(qint64 timeMs, quint64* realPixmapTimeMs = 0);

    void lockPixmaps();
    void unlockPixmaps();

signals:
    void gotNewPixmap(qint64 timeMs, QPixmap pixmap);
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
private:
    void allocateScaleContext(int linesize, int width, int height, PixelFormat format);
    bool processFrame(const CLVideoDecoderOutput& outFrame);
private:
    QScopedPointer<QnRtspClientArchiveDelegate> m_rtspClient;
    QMap<qint64, QPixmap> m_images;
    QMap<qint64, QPixmap> m_newImages;
    QnResourcePtr m_resource;

    qint64 m_step;
    qint64 m_startTime;
    qint64 m_endTime;
    int m_outWidth;
    int m_outHeight;
    QQueue<QnTimePeriod> m_rangeToLoad;
    mutable QMutex m_mutex;
    SwsContext* m_scaleContext;
    quint8* m_rgbaBuffer;
    int m_srcLineSize;
    int m_srcWidth;
    int m_srcHeight;
    int m_srcFormat;
    int m_prevOutWidth;
    int m_prevOutHeight;
    qint64 m_lastLoadedTime;
    bool m_breakCurrentJob;
};

#endif // __THUMBNAILS_LOADER_H__

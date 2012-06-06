#ifndef QN_THUMBNAILS_LOADER_H
#define QN_THUMBNAILS_LOADER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QMetaType>
#include <QtCore/QMutex>

#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "utils/media/frame_info.h"

typedef QSharedPointer<QPixmap> QPixmapPtr;

class QnThumbnailsLoader: public CLLongRunnable {
    Q_OBJECT;

    typedef CLLongRunnable base_type;

public:
    QnThumbnailsLoader(QnResourcePtr resource);
    virtual ~QnThumbnailsLoader();

    void setBoundingSize(const QSize &size);
    QSize boundingSize() const;
    
    QSize thumbnailSize() const;

    /**
     * Load video pixmaps by specified time
     */
    void loadRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs);

    QnTimePeriod loadedRange() const;
    
    qint64 currentMSecsSinceLastUpdate() const;

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
    QPixmapPtr getPixmapByTime(qint64 timeMs, qint64 *realPixmapTimeMs = 0);

    virtual void pleaseStop() override;

signals:
    void gotNewPixmap(qint64 timeMs, QPixmapPtr pixmap);

protected:
    virtual void run() override;

private:
    void ensureScaleContext(int lineSize, const QSize &size, PixelFormat format);
    bool processFrame(const CLVideoDecoderOutput &outFrame);

private:
    mutable QMutex m_mutex;
    QScopedPointer<QnRtspClientArchiveDelegate> m_rtspClient;
    QnResourcePtr m_resource;

    QMap<qint64, QPixmapPtr> m_images;
    QMap<qint64, QPixmapPtr> m_newImages;

    qint64 m_step;
    qint64 m_startTime;
    qint64 m_endTime;
    QQueue<QnTimePeriod> m_rangeToLoad;
    SwsContext *m_scaleContext;
    quint8 *m_rgbaBuffer;

    int m_srcLineSize;
    int m_srcFormat;
    QSize m_srcSize;
    QSize m_boundingSize;
    QSize m_dstSize;

    qint64 m_lastLoadedTime;
    bool m_breakCurrentJob;
};

Q_DECLARE_METATYPE(QPixmapPtr)

#endif // __THUMBNAILS_LOADER_H__

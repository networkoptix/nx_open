#ifndef QN_THUMBNAILS_LOADER_H
#define QN_THUMBNAILS_LOADER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QMetaType>
#include <QtCore/QMutex>

#include <utils/common/longrunnable.h>
#include <core/resource/resource_fwd.h>

#include "thumbnail.h"

class CLVideoDecoderOutput;
class QnRtspClientArchiveDelegate;
class SwsContext;

typedef QSharedPointer<QPixmap> QPixmapPtr;

class QnThumbnailsLoader: public CLLongRunnable {
    Q_OBJECT;

    typedef CLLongRunnable base_type;

public:
    QnThumbnailsLoader(QnResourcePtr resource);
    virtual ~QnThumbnailsLoader();

    QnResourcePtr resource() const;

    void setBoundingSize(const QSize &size);
    QSize boundingSize() const;
    
    QSize thumbnailSize() const;

    qint64 timeStep() const;
    void setTimeStep(qint64 timeStep);

    qint64 startTime() const;
    void setStartTime(qint64 startTime) const;

    qint64 endTime() const;
    void setEndTime(qint64 endTime);

    virtual void pleaseStop() override;

signals:
    void thumbnailLoaded(QnThumbnail thumbnail);
    void thumbnailSizeChanged();
    void timeStepChanged();

protected:
    virtual void run() override;

private:
    void ensureScaleContext(int lineSize, const QSize &size, const QSize &boundingSize, int format);
    bool processFrame(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize);

private:
    mutable QMutex m_mutex;
    const QnResourcePtr m_resource;
    const QScopedPointer<QnRtspClientArchiveDelegate> m_rtspClient;

    qint64 m_startTime, m_endTime, m_timeStep;

    QSize m_boundingSize;
    
    SwsContext *m_scaleContext;
    quint8 *m_scaleBuffer;
    QSize m_scaleSourceSize;
    QSize m_scaleTargetSize;
    int m_scaleSourceLine;
    int m_scaleSourceFormat;
};

Q_DECLARE_METATYPE(QPixmapPtr)

#endif // __THUMBNAILS_LOADER_H__

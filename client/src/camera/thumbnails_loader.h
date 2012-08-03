#ifndef QN_THUMBNAILS_LOADER_H
#define QN_THUMBNAILS_LOADER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QMetaType>
#include <QtCore/QMutex>
#include <QtCore/QStack>

#include <utils/common/longrunnable.h>
#include <core/resource/resource_fwd.h>

#include "recording/time_period.h"
#include "thumbnail.h"

#include "plugins/resources/archive/abstract_archive_delegate.h"

class CLVideoDecoderOutput;
class QnRtspClientArchiveDelegate;
class QnThumbnailsLoaderHelper;
struct SwsContext;



class QnThumbnailsLoader: public QnLongRunnable {
    Q_OBJECT;

    typedef QnLongRunnable base_type;

public:
    QnThumbnailsLoader(QnResourcePtr resource);
    virtual ~QnThumbnailsLoader();

    QnResourcePtr resource() const;

    void setBoundingSize(const QSize &size);
    QSize boundingSize() const;
    
    QSize sourceSize() const;
    QSize thumbnailSize() const;

    qint64 timeStep() const;
    void setTimeStep(qint64 timeStep);

    qint64 startTime() const;
    void setStartTime(qint64 startTime);

    qint64 endTime() const;
    void setEndTime(qint64 endTime);

    void setTimePeriod(qint64 startTime, qint64 endTime);
    void setTimePeriod(const QnTimePeriod &timePeriod);
    QnTimePeriod timePeriod() const;

    QHash<qint64, QnThumbnail> thumbnails() const;

    virtual void pleaseStop() override;

signals:
    void sourceSizeChanged();
    void thumbnailLoaded(const QnThumbnail &thumbnail);
    void thumbnailsInvalidated();

protected:
    virtual void run() override;

    void updateTargetSizeLocked(bool invalidate);
    void invalidateThumbnailsLocked();
    Q_SIGNAL void updateProcessingLater();
    Q_SLOT void updateProcessing();
    void updateProcessingLocked();

    void enqueueForProcessingLocked(qint64 startTime, qint64 endTime);
    Q_SIGNAL void processingRequested();
    void process();

    Q_SLOT void addThumbnail(const QnThumbnail &thumbnail);

    QnThumbnail generateThumbnail(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize, qint64 timeStep, int generation);
    qint64 processThumbnail(const QnThumbnail &thumbnail, qint64 startTime, qint64 endTime, bool ignorePeriod);

private:
    void ensureScaleContextLocked(int lineSize, const QSize &sourceSize, const QSize &boundingSize, int format);
    bool processFrame(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize);

    void setTimePeriodLocked(qint64 startTime, qint64 endTime);

    bool isProcessingPeriodValid() const;
    qreal getAspectRatio();
private:
    friend class QnThumbnailsLoaderHelper;

    mutable QMutex m_mutex;
    const QnResourcePtr m_resource;
    QList<QnAbstractArchiveDelegatePtr> m_delegates;

    qint64 m_timeStep, m_requestStart, m_requestEnd, m_processingStart, m_processingEnd;

    QSize m_boundingSize;
    QSize m_targetSize;
    
    SwsContext *m_scaleContext;
    quint8 *m_scaleBuffer;
    QSize m_scaleSourceSize;
    QSize m_scaleTargetSize;
    int m_scaleSourceLine;
    int m_scaleSourceFormat;
    
    QHash<qint64, QnThumbnail> m_thumbnailByTime;
    qint64 m_maxLoadedTime;
    QStack<QnTimePeriod> m_processingStack;
    QnThumbnailsLoaderHelper *m_helper;
    int m_generation;
    qreal m_cachedAspectRatio;
};

#endif // __THUMBNAILS_LOADER_H__


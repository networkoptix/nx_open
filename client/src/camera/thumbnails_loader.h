#ifndef QN_THUMBNAILS_LOADER_H
#define QN_THUMBNAILS_LOADER_H

#include <QtCore/QScopedPointer>
#include <QtCore/QMetaType>
#include <QtCore/QMutex>
#include <QtCore/QStack>

#include <utils/common/long_runnable.h>
#include <core/resource/resource_fwd.h>

#include "thumbnail.h"

#include "plugins/resource/archive/abstract_archive_delegate.h"

class CLVideoDecoderOutput;
class QnRtspClientArchiveDelegate;
class QnThumbnailsLoaderHelper;
class QnTimePeriod;

struct SwsContext;



class QnThumbnailsLoader: public QnLongRunnable {
    Q_OBJECT;

    typedef QnLongRunnable base_type;

public:
    enum class Mode {
        Default,                        /**< Default mode is used to load thumbnails over the timeline. */
        Strict                          /**< Strict mode is used in preview search. */
    };

    /** 
     *  Thumbnail loader constructor.
     *  \param resource                 Target camera or archive file
     *  \param mode                     Working mode. In default mode loader will adjust given time periods (add margins, etc) to show thumbnails 
     *                                  from the middle of the each given period. In strict mode thumbnail is taken from the beginning of the period.
     */
    QnThumbnailsLoader(const QnResourcePtr &resource, Mode mode = Mode::Default);
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
    const Mode m_mode;

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


#ifndef __QN_ABSTRACT_CAMERA_DATA_LOADER_H__
#define __QN_ABSTRACT_CAMERA_DATA_LOADER_H__

#include <QtCore/QObject>
#include <QtGui/QRegion>
#include <QtCore/QList>

#include <camera/data/camera_data_fwd.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

class QnTimePeriod;

/** Base class for loading custom camera archive-related data. */
class QnAbstractCameraDataLoader: public QObject
{
    Q_OBJECT
public:
    QnAbstractCameraDataLoader(const QnResourcePtr &resource, const Qn::CameraDataType dataType, QObject *parent);
    virtual ~QnAbstractCameraDataLoader();

    /**
     * \param timePeriod                Time period to get data for.
     * \param filter                    Custom data filter.
     * \param resolutionMs              Minimal length of the data period that should be loaded, in milliseconds.
     * \returns                         Request handle.
     */
    virtual int load(const QnTimePeriod &period, const QString &filter = QString(), const qint64 resolutionMs = 1) = 0;

    /**
     * \returns                         Resource that this loader works with.
     */
    QnResourcePtr resource() const { return m_resource; }

    /**
     * Discards cached data, if any.
     * 
     * \param resolutionMs              Resolution, data for which should be discarded. All data should be discarded if parameter equals to 0 (default).
     */
    Q_SLOT virtual void discardCachedData(const qint64 resolutionMs = 0);

signals:
    /**
     * This signal is emitted whenever motion periods were successfully loaded.
     *
     * \param timePeriods               Loaded motion periods.
     * \param handle                    Request handle.
     */
    void ready(const QnAbstractCameraDataPtr &data, int handle);

    /**
     * This signal is emitted whenever the reader was unable to load motion periods.
     * 
     * \param status                    Error code.
     * \param handle                    Request handle.
     */
    void failed(int status, int handle);

protected:
    /** Resource that this loader gets chunks for. */
    const QnResourcePtr m_resource;

    const Qn::CameraDataType m_dataType;

signals:
    void delayedReady(const QnAbstractCameraDataPtr &data, int handle);
};

#endif // __QN_ABSTRACT_CAMERA_DATA_LOADER_H__

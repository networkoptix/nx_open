#ifndef QN_ABSTRACT_ASYNC_PTZ_CONTOLLER_H
#define QN_ABSTRACT_ASYNC_PTZ_CONTOLLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnAbstractAsyncPtzController: public QObject {
    Q_OBJECT
public:
    QnAbstractAsyncPtzController(const QnResourcePtr &resource);
    virtual ~QnAbstractAsyncPtzController();

    /**
     * \returns                         Resource that this ptz controller belongs to.
     */
    const QnResourcePtr &resource() const { return m_resource; }

    virtual Qn::PtzCapabilities getCapabilities() = 0;
    bool hasCapabilities(Qn::PtzCapabilities capabilities) { return (getCapabilities() & capabilities) == capabilities; }

    virtual int continuousMove(const QVector3D &speed, QObject *target, const char *slot) = 0;
    virtual int absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, QObject *target, const char *slot) = 0;
    virtual int viewportMove(qreal aspectRatio, const QRectF &viewport, QObject *target, const char *slot) = 0;
    
    virtual int getFlip(QObject *target, const char *slot) = 0;
    virtual int getLimits(QObject *target, const char *slot) = 0;
    virtual int getPosition(Qn::PtzCoordinateSpace space, QObject *target, const char *slot) = 0;

    virtual int createPreset(const QnPtzPreset &preset, QObject *target, const char *slot) = 0;
    virtual int updatePreset(const QnPtzPreset &preset, QObject *target, const char *slot) = 0;
    virtual int removePreset(const QString &presetId, QObject *target, const char *slot) = 0;
    virtual int activatePreset(const QString &presetId, QObject *target, const char *slot) = 0;
    virtual int getPresets(QObject *target, const char *slot) = 0;

    virtual int createTour(const QnPtzTour &tour, QObject *target, const char *slot) = 0;
    virtual int removeTour(const QString &tourId, QObject *target, const char *slot) = 0;
    virtual int activateTour(const QString &tourId, QObject *target, const char *slot) = 0;
    virtual int getTours(QObject *target, const char *slot) = 0;

private:
    QnResourcePtr m_resource;
};

#endif // QN_ABSTRACT_ASYNC_PTZ_CONTOLLER_H

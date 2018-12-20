#ifndef QN_ACTI_PTZ_CONTROLLER_H
#define QN_ACTI_PTZ_CONTROLLER_H

#ifdef ENABLE_ACTI

#include <QtCore/QScopedPointer>

#include <core/ptz/basic_ptz_controller.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnActiPtzControllerPrivate;

class QnActiPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnActiPtzController(const QnActiResourcePtr &resource);
    virtual ~QnActiPtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getFlip(
        Qt::Orientations *flip,
        const nx::core::ptz::Options& options) const override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition,
        const nx::core::ptz::Options& options) const override;

private:
    QScopedPointer<QnActiPtzControllerPrivate> d;
};

#endif // ENABLE_ACTI
#endif // QN_ACTI_PTZ_CONTROLLER_H

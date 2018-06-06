#ifndef QN_ACTI_PTZ_CONTROLLER_H
#define QN_ACTI_PTZ_CONTROLLER_H

#ifdef ENABLE_ACTI

#include <QtCore/QScopedPointer>

#include <core/ptz/basic_ptz_controller.h>

class QnActiPtzControllerPrivate;

class QnActiPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnActiPtzController(const QnActiResourcePtr &resource);
    virtual ~QnActiPtzController();

    virtual Ptz::Capabilities getCapabilities() const override;
    virtual bool continuousMove(const nx::core::ptz::Vector& speed) override;
    virtual bool getFlip(Qt::Orientations *flip) const override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const nx::core::ptz::Vector& position, qreal speed) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, nx::core::ptz::Vector* outPosition) const override;

private:
    QScopedPointer<QnActiPtzControllerPrivate> d;
};

#endif // ENABLE_ACTI
#endif // QN_ACTI_PTZ_CONTROLLER_H

#ifndef QN_PROXY_PTZ_CONTROLLER_H
#define QN_PROXY_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnProxyPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnProxyPtzController(const QnPtzControllerPtr &baseController):
        QnAbstractPtzController(baseController->resource()),
        m_baseController(baseController)
    {}

    QnPtzControllerPtr baseController() const {
        return m_baseController;
    }

    virtual Qn::PtzCapabilities getCapabilities() override                          { return m_baseController->getCapabilities(); }
    virtual bool continuousMove(const QVector3D &speed) override                    { return m_baseController->continuousMove(speed); }
    virtual bool getFlip(Qt::Orientations *flip) override                           { return m_baseController->getFlip(flip); }
    virtual bool absoluteMove(const QVector3D &position) override                   { return m_baseController->absoluteMove(position); }
    virtual bool getPosition(QVector3D *position) override                          { return m_baseController->getPosition(position); }
    virtual bool getLimits(QnPtzLimits *limits) override                            { return m_baseController->getLimits(limits); }
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override   { return m_baseController->relativeMove(aspectRatio, viewport); }

private:
    QnPtzControllerPtr m_baseController;
};


#endif // QN_PROXY_PTZ_CONTROLLER_H

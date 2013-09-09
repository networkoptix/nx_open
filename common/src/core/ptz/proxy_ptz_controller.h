#ifndef QN_PROXY_PTZ_CONTROLLER_H
#define QN_PROXY_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnProxyPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnProxyPtzController(const QnPtzControllerPtr &baseController):
        QnAbstractPtzController(baseController->resource())
    {}

    QnPtzControllerPtr baseController() const {
        return m_baseController;
    }

    virtual Qn::PtzCapabilities getCapabilities() override      { return m_baseController->getCapabilities(); }
    virtual int startMove(const QVector3D &speed) override      { return m_baseController->startMove(speed); }
    virtual int stopMove() override                             { return m_baseController->stopMove(); }
    virtual int getFlip(Qt::Orientations *flip) override        { return m_baseController->getFlip(flip); }
    virtual int setPosition(const QVector3D &position) override { return m_baseController->setPosition(position); }
    virtual int getPosition(QVector3D *position) override       { return m_baseController->getPosition(position); }
    virtual int getLimits(QnPtzLimits *limits) override         { return m_baseController->getLimits(limits); }
    virtual int relativeMove(const QRectF &viewport) override   { return m_baseController->relativeMove(viewport); }

private:
    QnPtzControllerPtr m_baseController;
};


#endif // QN_PROXY_PTZ_CONTROLLER_H

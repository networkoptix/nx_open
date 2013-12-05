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

    virtual Qn::PtzCapabilities getCapabilities() override                                      { return m_baseController->getCapabilities(); }

    virtual bool continuousMove(const QVector3D &speed) override                                { return m_baseController->continuousMove(speed); }
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override { return m_baseController->absoluteMove(space, position); }
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override               { return m_baseController->relativeMove(aspectRatio, viewport); }

    virtual bool getFlip(Qt::Orientations *flip) override                                       { return m_baseController->getFlip(flip); }
    virtual bool getLimits(QnPtzLimits *limits) override                                        { return m_baseController->getLimits(limits); }
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override        { return m_baseController->getPosition(space, position); }

    virtual bool addPreset(const QnPtzPreset &preset) override                                  { return m_baseController->addPreset(preset); }
    virtual bool removePreset(const QnPtzPreset &preset) override                               { return m_baseController->removePreset(preset); }
    virtual bool activatePreset(const QnPtzPreset &preset) override                             { return m_baseController->activatePreset(preset); }
    virtual bool getPresets(QnPtzPresetList *presets) override                                  { return m_baseController->getPresets(presets); }

private:
    QnPtzControllerPtr m_baseController;
};


#endif // QN_PROXY_PTZ_CONTROLLER_H

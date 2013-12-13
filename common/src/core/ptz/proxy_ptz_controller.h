#ifndef QN_PROXY_PTZ_CONTROLLER_H
#define QN_PROXY_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnProxyPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnProxyPtzController(const QnPtzControllerPtr &baseController):
        QnAbstractPtzController(baseController->resource()),
        m_baseController(baseController),
        m_baseIsBlocking(!baseController->hasCapabilities(Qn::NonBlockingPtzCapability))
    {
        if(!m_baseIsBlocking)
            connect(m_baseController.data(), &QnAbstractPtzController::synchronized, this, &QnProxyPtzController::synchronizeProxy, Qt::QueuedConnection);
    }

    virtual ~QnProxyPtzController() {
        if(!m_baseIsBlocking)
            disconnect(m_baseController.data(), NULL, this, NULL);
    }

    QnPtzControllerPtr baseController() const                                                                   { return m_baseController; }

    virtual Qn::PtzCapabilities getCapabilities() override                                                      { return m_baseController->getCapabilities(); }

    virtual bool continuousMove(const QVector3D &speed) override                                                { return m_baseController->continuousMove(speed); }
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override    { return m_baseController->absoluteMove(space, position, speed); }
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override                  { return m_baseController->viewportMove(aspectRatio, viewport, speed); }

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override                        { return m_baseController->getPosition(space, position); }
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override                          { return m_baseController->getLimits(space, limits); }
    virtual bool getFlip(Qt::Orientations *flip) override                                                       { return m_baseController->getFlip(flip); }

    virtual bool createPreset(const QnPtzPreset &preset) override                                               { return m_baseController->createPreset(preset); }
    virtual bool updatePreset(const QnPtzPreset &preset) override                                               { return m_baseController->updatePreset(preset); }
    virtual bool removePreset(const QString &presetId) override                                                 { return m_baseController->removePreset(presetId); }
    virtual bool activatePreset(const QString &presetId, qreal speed) override                                  { return m_baseController->activatePreset(presetId, speed); }
    virtual bool getPresets(QnPtzPresetList *presets) override                                                  { return m_baseController->getPresets(presets); }

    virtual bool createTour(const QnPtzTour &tour) override                                                     { return m_baseController->createTour(tour); }
    virtual bool removeTour(const QString &tourId) override                                                     { return m_baseController->removeTour(tourId); }
    virtual bool activateTour(const QString &tourId) override                                                   { return m_baseController->activateTour(tourId); }
    virtual bool getTours(QnPtzTourList *tours) override                                                        { return m_baseController->getTours(tours); }

    virtual void synchronize(Qn::PtzDataFields query) override { 
        m_baseController->synchronize(query);
        if(m_baseIsBlocking) {
            QnPtzData data;
            m_baseController->getData(query, &data);
            synchronizeProxy(data); 
        }
    }

    Q_SLOT virtual void synchronizeProxy(const QnPtzData &data) {
        emit synchronized(data); 
    }

private:
    QnPtzControllerPtr m_baseController;
    bool m_baseIsBlocking;
};


#endif // QN_PROXY_PTZ_CONTROLLER_H

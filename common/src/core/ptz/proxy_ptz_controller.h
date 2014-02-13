#ifndef QN_PROXY_PTZ_CONTROLLER_H
#define QN_PROXY_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

/**
 * A simple controller that proxies all requests into another controller.
 */
class QnProxyPtzController: public QnAbstractPtzController {
    Q_OBJECT;
    typedef QnAbstractPtzController base_type;

public:
    QnProxyPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnProxyPtzController();

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

    virtual bool getActiveObject(QnPtzObject *activeObject) override                                            { return m_baseController->getActiveObject(activeObject); }
    virtual bool updateHomeObject(const QnPtzObject &homeObject) override                                       { return m_baseController->updateHomeObject(homeObject); }
    virtual bool getHomeObject(QnPtzObject *homeObject) override                                                { return m_baseController->getHomeObject(homeObject); }

    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data) override                                     { return base_type::getData(query, data); /* This is important because of base implementation! */ }

protected:
    virtual void baseFinished(Qn::PtzCommand command, const QVariant &data)                                     { emit finished(command, data); }
    virtual void baseChanged(Qn::PtzDataFields fields)                                                          { emit changed(fields); }

    Q_SIGNAL void finishedLater(Qn::PtzCommand command, const QVariant &data);

private:
    QnPtzControllerPtr m_baseController;
};


#endif // QN_PROXY_PTZ_CONTROLLER_H

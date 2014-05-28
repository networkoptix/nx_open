#ifndef QN_FALLBACK_PTZ_CONTROLLER_H
#define QN_FALLBACK_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnFallbackPtzController: public QnAbstractPtzController {
    Q_OBJECT;
    typedef QnAbstractPtzController base_type;

public:
    QnFallbackPtzController(const QnPtzControllerPtr &mainController, const QnPtzControllerPtr &fallbackController);
    virtual ~QnFallbackPtzController();

    QnPtzControllerPtr mainController() const                                                                   { return m_mainController; }
    QnPtzControllerPtr fallbackController() const                                                               { return m_fallbackController; }

    virtual Qn::PtzCapabilities getCapabilities() override                                                      { return baseController()->getCapabilities(); }

    virtual bool continuousMove(const QVector3D &speed) override                                                { return baseController()->continuousMove(speed); }
    virtual bool continuousFocus(qreal speed) override                                                          { return baseController()->continuousFocus(speed); }
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override    { return baseController()->absoluteMove(space, position, speed); }
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override                  { return baseController()->viewportMove(aspectRatio, viewport, speed); }

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override                        { return baseController()->getPosition(space, position); }
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override                          { return baseController()->getLimits(space, limits); }
    virtual bool getFlip(Qt::Orientations *flip) override                                                       { return baseController()->getFlip(flip); }

    virtual bool createPreset(const QnPtzPreset &preset) override                                               { return baseController()->createPreset(preset); }
    virtual bool updatePreset(const QnPtzPreset &preset) override                                               { return baseController()->updatePreset(preset); }
    virtual bool removePreset(const QString &presetId) override                                                 { return baseController()->removePreset(presetId); }
    virtual bool activatePreset(const QString &presetId, qreal speed) override                                  { return baseController()->activatePreset(presetId, speed); }
    virtual bool getPresets(QnPtzPresetList *presets) override                                                  { return baseController()->getPresets(presets); }

    virtual bool createTour(const QnPtzTour &tour) override                                                     { return baseController()->createTour(tour); }
    virtual bool removeTour(const QString &tourId) override                                                     { return baseController()->removeTour(tourId); }
    virtual bool activateTour(const QString &tourId) override                                                   { return baseController()->activateTour(tourId); }
    virtual bool getTours(QnPtzTourList *tours) override                                                        { return baseController()->getTours(tours); }

    virtual bool getActiveObject(QnPtzObject *activeObject) override                                            { return baseController()->getActiveObject(activeObject); }
    virtual bool updateHomeObject(const QnPtzObject &homeObject) override                                       { return baseController()->updateHomeObject(homeObject); }
    virtual bool getHomeObject(QnPtzObject *homeObject) override                                                { return baseController()->getHomeObject(homeObject); }

    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data) override                                     { return baseController()->getData(query, data); }

protected:
    void baseFinished(Qn::PtzCommand command, const QVariant &data)                                             { emit finished(command, data); }
    void baseChanged(Qn::PtzDataFields fields);

private:
    const QnPtzControllerPtr &baseController()                                                                  { return m_mainIsValid ? m_mainController : m_fallbackController; }

private:
    bool m_mainIsValid;
    QnPtzControllerPtr m_mainController;
    QnPtzControllerPtr m_fallbackController;
};

#endif // QN_FALLBACK_PTZ_CONTROLLER_H

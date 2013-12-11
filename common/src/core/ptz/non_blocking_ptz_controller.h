#ifndef QN_NON_BLOCKING_PTZ_CONTROLLER_H
#define QN_NON_BLOCKING_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnNonBlockingPtzControllerPrivate;

class QnPtzCommandBase: public QObject {
    Q_OBJECT
signals:
    void finished(Qn::PtzDataFields fields, bool status, const QVariant &result);
};

class QnNonBlockingPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;
public:
    QnNonBlockingPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnNonBlockingPtzController();

    static bool extends(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
        
    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

    virtual bool createTour(const QnPtzTour &tour) override;
    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;
    virtual bool getTours(QnPtzTourList *tours) override;

    virtual void synchronize(Qn::PtzDataFields fields) override;

protected:
    bool hasSpaceCapabilities(Qn::PtzCapabilities capabilities, Qn::PtzCoordinateSpace space) const;

    template<class Functor>
    void runCommand(Qn::PtzDataFields fields, const Functor &functor);

    Q_SLOT void at_ptzCommand_finished(Qn::PtzDataFields fields, bool status, const QVariant &result);

private:
    QScopedPointer<QnNonBlockingPtzControllerPrivate> d;
};

#endif // QN_NON_BLOCKING_PTZ_CONTROLLER_H

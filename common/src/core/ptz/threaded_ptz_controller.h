#ifndef QN_THREADED_PTZ_CONTROLLER_H
#define QN_THREADED_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnThreadedPtzControllerPrivate;

class QnPtzCommandBase: public QObject {
    Q_OBJECT
signals:
    void finished(Qn::PtzCommand command, const QVariant &data);
};


class QnThreadedPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;
public:
    QnThreadedPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnThreadedPtzController();

    static bool extends(Qn::PtzCapabilities capabilities);

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

    virtual bool getActiveObject(QnPtzObject *activeObject) override;
    virtual bool updateHomeObject(const QnPtzObject &homeObject) override;
    virtual bool getHomeObject(QnPtzObject *homeObject) override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data) override;

private:
    template<class Functor>
    void runCommand(Qn::PtzCommand command, const Functor &functor) const;

private:
    QScopedPointer<QnThreadedPtzControllerPrivate> d;
};

#endif // QN_THREADED_PTZ_CONTROLLER_H

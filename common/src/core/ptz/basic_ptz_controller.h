#ifndef QN_BASIC_PTZ_CONTROLLER_H
#define QN_BASIC_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnBasicPtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnBasicPtzController(const QnResourcePtr &resource): base_type(resource) {}

    virtual Qn::PtzCapabilities getCapabilities() override                                  { return Qn::NoPtzCapabilities; }

    virtual bool continuousMove(const QVector3D &) override                                 { return false; }
    virtual bool absoluteMove(Qn::PtzCoordinateSpace, const QVector3D &, qreal) override    { return false; }
    virtual bool viewportMove(qreal, const QRectF &, qreal) override                        { return false; }

    virtual bool getPosition(Qn::PtzCoordinateSpace, QVector3D *) override                  { return false; }
    virtual bool getLimits(Qn::PtzCoordinateSpace, QnPtzLimits *) override                  { return false; }
    virtual bool getFlip(Qt::Orientations *) override                                       { return false; }

    virtual bool createPreset(const QnPtzPreset &) override                                 { return false; }
    virtual bool updatePreset(const QnPtzPreset &) override                                 { return false; }
    virtual bool removePreset(const QString &) override                                     { return false; }
    virtual bool activatePreset(const QString &, qreal) override                            { return false; }
    virtual bool getPresets(QnPtzPresetList *) override                                     { return false; }

    virtual bool createTour(const QnPtzTour &) override                                     { return false; }
    virtual bool removeTour(const QString &) override                                       { return false; }
    virtual bool activateTour(const QString &) override                                     { return false; }
    virtual bool getTours(QnPtzTourList *) override                                         { return false; }

    virtual bool getActiveObject(QnPtzObject *) override                                    { return false; }
    virtual bool updateHomeObject(const QnPtzObject &) override                             { return false; }
    virtual bool getHomeObject(QnPtzObject *) override                                      { return false; }
};

#endif // QN_BASIC_PTZ_CONTROLLER_H

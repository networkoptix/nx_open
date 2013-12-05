#ifndef QN_BASIC_PTZ_CONTROLLER_H
#define QN_BASIC_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

class QnBasicPtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnBasicPtzController(const QnResourcePtr &resource): base_type(resource) {}

    virtual Qn::PtzCapabilities getCapabilities() override                          { return Qn::NoPtzCapabilities; }

    virtual bool continuousMove(const QVector3D &) override                         { return false; }
    virtual bool absoluteMove(Qn::PtzCoordinateSpace, const QVector3D &) override   { return false; }
    virtual bool viewportMove(qreal, const QRectF &) override                       { return false; }

    virtual bool getFlip(Qt::Orientations *) override                               { return false; }
    virtual bool getLimits(QnPtzLimits *) override                                  { return false; }
    virtual bool getPosition(Qn::PtzCoordinateSpace, QVector3D *) override          { return false; }

    virtual bool createPreset(QnPtzPreset *) override                               { return false; }
    virtual bool removePreset(const QnPtzPreset &) override                         { return false; }
    virtual bool activatePreset(const QnPtzPreset &) override                       { return false; }
    virtual bool getPresets(QnPtzPresetList *) override                             { return false; }
};

#endif // QN_BASIC_PTZ_CONTROLLER_H

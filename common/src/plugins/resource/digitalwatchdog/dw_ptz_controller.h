#ifndef QN_DW_PTZ_CONTROLLER_H
#define QN_DW_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_ptz_controller.h>

class QnDwPtzController: public QnOnvifPtzController {
    Q_OBJECT
    typedef QnOnvifPtzController base_type;

public:
    QnDwPtzController(const QnDigitalWatchdogResourcePtr &resource);
    virtual ~QnDwPtzController();

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
private slots:
    void at_physicalParamChanged(const QString& id, const QString& value);
    void updateFlipState();
private:
    QnDigitalWatchdogResourcePtr m_resource;
    Qt::Orientations m_flip;
};

#endif // ENABLE_ONVIF

#endif // QN_DW_PTZ_CONTROLLER_H

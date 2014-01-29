#ifndef QN_ACTI_PTZ_CONTROLLER_H
#define QN_ACTI_PTZ_CONTROLLER_H

#ifdef ENABLE_ACTI

#include <QtCore/QMutex>

#include <core/ptz/basic_ptz_controller.h>


class QnActiPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnActiPtzController(const QnActiResourcePtr &resource);
    virtual ~QnActiPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    
private:
    void init();
    bool continuousZoomInternal(int deviceZoomSpeed);
    bool continuousMoveInternal(int devicePanSpeed, int deviceTiltSpeed);

    bool query(const QString &request, QByteArray *body = NULL, bool keepAllData = false) const;

    static int toDeviceZoomSpeed(qreal zoomSpeed);
    static int toDevicePanTiltSpeed(qreal panTiltSpeed);
    static QString zoomDirection(int deviceZoomSpeed);
    static QString panTiltDirection(int devicePanSpeed, int deviceTiltSpeed);

private:
    QMutex m_mutex;
    QnActiResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;

    bool m_isFlipped;
    bool m_isMirrored;

    int m_deviceZoomSpeed, m_devicePanSpeed, m_deviceTiltSpeed;
};

#endif // #ifdef ENABLE_ACTI
#endif // QN_ACTI_PTZ_CONTROLLER_H

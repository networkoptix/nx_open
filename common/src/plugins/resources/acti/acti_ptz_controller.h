#ifndef QN_ACTI_PTZ_CONTROLLER_H
#define QN_ACTI_PTZ_CONTROLLER_H

#ifdef ENABLE_ACTI

#include <QtCore/QHash>
#include <QtCore/QMutex>

#include <core/ptz/basic_ptz_controller.h>

class CLSimpleHTTPClient;
class QnActiParameterMap;

class QnActiPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnActiPtzController(const QnActiResourcePtr &resource);
    virtual ~QnActiPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    
private:
    void init();
    bool startZoomInternal(qreal zoomVelocity);
    bool startMoveInternal(qreal xVelocity, qreal yVelocity);
    bool stopZoomInternal();
    bool stopMoveInternal();

private:
    QMutex m_mutex;
    QnActiResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    //QnPtzSpaceMapper *m_spaceMapper;

    qreal m_zoomVelocity;
    QPair<int, int> m_moveVelocity;
    qreal m_minAngle;
    qreal m_maxAngle;
    bool m_isFlipped;
    bool m_isMirrored;
};

#endif // #ifdef ENABLE_ACTI
#endif // QN_ACTI_PTZ_CONTROLLER_H

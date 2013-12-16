#ifndef QN_ACTI_PTZ_CONTROLLER_H
#define QN_ACTI_PTZ_CONTROLLER_H

#ifdef ENABLE_ACTI

#include <QtCore/QHash>
#include <QtCore/QMutex>

#include <core/resource/interface/abstract_ptz_controller.h>

class CLSimpleHTTPClient;
class QnActiParameterMap;

class QnActiPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnActiPtzController(QnActiResource* resource);
    virtual ~QnActiPtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual int stopMove() override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

private:
    void init();
    int startZoomInternal(qreal zoomVelocity);
    int startMoveInternal(qreal xVelocity, qreal yVelocity);
    int stopZoomInternal();
    int stopMoveInternal();
private:
    QMutex m_mutex;
    QnActiResource* m_resource;
    Qn::PtzCapabilities m_capabilities;
    QnPtzSpaceMapper *m_spaceMapper;

    qreal m_zoomVelocity;
    QPair<int, int> m_moveVelocity;
    qreal m_minAngle;
    qreal m_maxAngle;
    bool m_isFliped;
    bool m_isMirrored;
};

#endif // #ifdef ENABLE_ACTI
#endif // QN_ACTI_PTZ_CONTROLLER_H

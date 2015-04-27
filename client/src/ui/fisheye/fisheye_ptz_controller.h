#ifndef QN_FISHEYE_PTZ_CONTROLLER_H
#define QN_FISHEYE_PTZ_CONTROLLER_H

#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <core/ptz/basic_ptz_controller.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/animation/animation_timer_listener.h>

class QnResourceWidgetRenderer;
class QnMediaResourceWidget;

class QnFisheyePtzController: public QnBasicPtzController, public AnimationTimerListener {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnFisheyePtzController(QnMediaResourceWidget *widget);
    virtual ~QnFisheyePtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

    QnMediaDewarpingParams mediaDewarpingParams() const;
    QnItemDewarpingParams itemDewarpingParams() const;

    static QVector3D positionFromRect(const QnMediaDewarpingParams &dewarpingParams, const QRectF &rect);
    qreal customAR() const;
protected:
    virtual void tick(int deltaMSecs) override;

private:
    Q_SLOT void updateLimits();
    Q_SLOT void updateCapabilities();
    Q_SLOT void updateAspectRatio();

    Q_SLOT void updateMediaDewarpingParams();
    Q_SLOT void updateItemDewarpingParams();

    QVector3D boundedPosition(const QVector3D &position);
    QVector3D getPositionInternal();
    void absoluteMoveInternal(const QVector3D &position);

private:
    enum AnimationMode {
        NoAnimation,
        SpeedAnimation,
        PositionAnimation
    };

    QPointer<QnMediaResourceWidget> m_widget;
    QPointer<QnResourceWidgetRenderer> m_renderer;
    Qn::PtzCapabilities m_capabilities;
    QVector3D m_unitSpeed;

    QnPtzLimits m_limits;
    bool m_unlimitedPan;
    
    qreal m_aspectRatio;

    AnimationMode m_animationMode;
    QVector3D m_speed;
    QVector3D m_startPosition;
    QVector3D m_endPosition;
    qreal m_relativeSpeed;
    qreal m_progress;

    QnMediaDewarpingParams m_mediaDewarpingParams;
    QnItemDewarpingParams m_itemDewarpingParams;
};

#endif // QN_FISHEYE_PTZ_CONTROLLER_H

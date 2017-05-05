#pragma once

#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <core/ptz/basic_ptz_controller.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/ptz_constants.h>

#include <ui/animation/animation_timer_listener.h>

class QnResourceWidgetRenderer;
class QnMediaResourceWidget;

class QnFisheyePtzController: public QnBasicPtzController, public AnimationTimerListener
{
    Q_OBJECT
    using base_type = QnBasicPtzController;

public:
    QnFisheyePtzController(QnMediaResourceWidget* widget);
    QnFisheyePtzController(const QnMediaResourcePtr& mediaRes);
    virtual ~QnFisheyePtzController();

    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const QVector3D& position,
        qreal speed) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

    QnMediaDewarpingParams mediaDewarpingParams() const;
    QnItemDewarpingParams itemDewarpingParams() const;

    static QVector3D positionFromRect(
        const QnMediaDewarpingParams& dewarpingParams,
        const QRectF& rect);

    qreal customAR() const;

protected:
    virtual void tick(int deltaMSecs) override;

private:
    Q_SLOT void updateLimits();
    Q_SLOT void updateCapabilities();
    Q_SLOT void updateAspectRatio();

    Q_SLOT void updateMediaDewarpingParams();
    Q_SLOT void updateItemDewarpingParams();

    QVector3D boundedPosition(const QVector3D& position);
    QVector3D getPositionInternal() const;
    void absoluteMoveInternal(const QVector3D& position);

private:
    enum AnimationMode
    {
        NoAnimation,
        SpeedAnimation,
        PositionAnimation
    };

    QPointer<QnMediaResourceWidget> m_widget;
    QPointer<QnResourceWidgetRenderer> m_renderer;
    Ptz::Capabilities m_capabilities;
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


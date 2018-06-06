#pragma once

#include <QtCore/QPointer>
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

    virtual bool continuousMove(const nx::core::ptz::Vector& speedVector) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition) const override;

    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

    QnMediaDewarpingParams mediaDewarpingParams() const;
    QnItemDewarpingParams itemDewarpingParams() const;

    static nx::core::ptz::Vector positionFromRect(
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

    nx::core::ptz::Vector boundedPosition(const nx::core::ptz::Vector& position);
    nx::core::ptz::Vector getPositionInternal() const;
    void absoluteMoveInternal(const nx::core::ptz::Vector& position);

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
    nx::core::ptz::Vector m_unitSpeed;

    QnPtzLimits m_limits;
    bool m_unlimitedPan;

    qreal m_aspectRatio;

    AnimationMode m_animationMode;
    nx::core::ptz::Vector m_speed;
    nx::core::ptz::Vector m_startPosition;
    nx::core::ptz::Vector m_endPosition;
    qreal m_relativeSpeed;
    qreal m_progress;

    QnMediaDewarpingParams m_mediaDewarpingParams;
    QnItemDewarpingParams m_itemDewarpingParams;
};


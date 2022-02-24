// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <core/ptz/basic_ptz_controller.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <core/ptz/ptz_constants.h>

#include <ui/animation/animation_timer_listener.h>
#include <utils/common/aspect_ratio.h>

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

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(
        const Vector& speedVector,
        const Options& options) override;

    virtual bool absoluteMove(
        CoordinateSpace space,
        const Vector& position,
        qreal speed,
        const Options& options) override;

    virtual bool getPosition(
        Vector* outPosition,
        CoordinateSpace space,
        const Options& options) const override;

    virtual bool getLimits(
        QnPtzLimits* limits,
        CoordinateSpace space,
        const Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const Options& options) const override;

    nx::vms::api::dewarping::MediaData mediaDewarpingParams() const;
    nx::vms::api::dewarping::ViewData itemDewarpingParams() const;

    static Vector positionFromRect(
        const nx::vms::api::dewarping::MediaData& dewarpingParams,
        const QRectF& rect);

    QnAspectRatio customAR() const;

    Q_SLOT void updateLimits();

protected:
    virtual void tick(int deltaMSecs) override;

private:
    Q_SLOT void updateCapabilities();
    Q_SLOT void updateAspectRatio();

    Q_SLOT void updateMediaDewarpingParams();
    Q_SLOT void updateItemDewarpingParams();

    Vector boundedPosition(const Vector& position);
    Vector getPositionInternal() const;
    void absoluteMoveInternal(const Vector& position);

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
    Vector m_unitSpeed;

    QnPtzLimits m_limits;
    bool m_unlimitedPan;

    qreal m_aspectRatio;

    AnimationMode m_animationMode;
    Vector m_speed;
    Vector m_startPosition;
    Vector m_endPosition;
    qreal m_relativeSpeed;
    qreal m_progress;

    nx::vms::api::dewarping::MediaData m_mediaDewarpingParams;
    nx::vms::api::dewarping::ViewData m_itemDewarpingParams;
};

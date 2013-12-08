#ifndef QN_FISHEYE_PTZ_CONTROLLER_H
#define QN_FISHEYE_PTZ_CONTROLLER_H

#include <QtCore/QPointer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <core/resource/dewarping_params.h>
#include <core/ptz/basic_ptz_controller.h>

class QnResourceWidgetRenderer;
class QnMediaResourceWidget;

class QnFisheyePtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnFisheyePtzController(QnMediaResourceWidget *widget);
    virtual ~QnFisheyePtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

    virtual bool getProjection(Qn::Projection *projection) override;
    virtual bool setProjection(Qn::Projection projection) override;

    void tick();

    const DewarpingParams &dewarpingParams() const;
    //void setDewarpingParams(const DewarpingParams &dewarpingParams);

    ///virtual void changePanoMode() override;
    ///virtual QString getPanoModeText() const override;

private:
    Q_SLOT void updateLimits();
    Q_SLOT void updateCapabilities();
    Q_SLOT void updateAspectRatio();
    Q_SLOT void updateDewarpingParams();

    QVector3D boundedPosition(const QVector3D &position);
    QVector3D getPositionInternal();
    void absoluteMoveInternal(const QVector3D &position);

private:
    QnMediaResourcePtr m_resource;
    QPointer<QnMediaResourceWidget> m_widget;
    QPointer<QnResourceWidgetRenderer> m_renderer;
    Qn::PtzCapabilities m_capabilities;

    QnPtzLimits m_limits;
    bool m_unlimitedPan;
    
    qreal m_aspectRatio;

    QElapsedTimer m_timer;
    bool m_animating;
    QVector3D m_speed;

    DewarpingParams m_dewarpingParams;
};

#endif // QN_FISHEYE_PTZ_CONTROLLER_H

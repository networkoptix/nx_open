#ifndef __FISHEYE_PTZ_CONTROLLER_H__
#define __FISHEYE_PTZ_CONTROLLER_H__

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

    void tick();

    const DewarpingParams &dewarpingParams() const;
    //void setDewarpingParams(const DewarpingParams &dewarpingParams);

    ///virtual void changePanoMode() override;
    ///virtual QString getPanoModeText() const override;

//signals:
    //void dewarpingParamsChanged();
    //void spaceMapperChanged();

private:
    QVector3D boundedPosition(const QVector3D &position);
    void updateLimits();
    void updateCapabilities();

private slots:
    void at_widget_aspectRatioChanged();
    void at_widget_dewarpingParamsChanged();

private:
    enum Animation {
        NoAnimation,
        ContinuousMoveAnimation,
        AbsoluteMoveAnimation
    };

    QnMediaResourcePtr m_resource;
    QPointer<QnMediaResourceWidget> m_widget;
    QPointer<QnResourceWidgetRenderer> m_renderer;
    Qn::PtzCapabilities m_capabilities;

    QnPtzLimits m_limits;
    bool m_unlimitedPan;
    
    qreal m_aspectRatio;

    QVector3D m_targetPosition;
    QVector3D m_sourcePosition;
    QVector3D m_speed;

    DewarpingParams m_dewarpingParams;
    
    Animation m_animation;

    QElapsedTimer m_timer;

    //bool m_moveToAnimation;
    //QnPtzSpaceMapper* m_spaceMapper;
};

#endif // __FISHEYE_PTZ_CONTROLLER_H__

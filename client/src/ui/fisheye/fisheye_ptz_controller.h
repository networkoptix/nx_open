#ifndef __FISHEYE_PTZ_CONTROLLER_H__
#define __FISHEYE_PTZ_CONTROLLER_H__

#include <QVector3D>

#include <core/ptz/basic_ptz_controller.h>
#include <core/resource/dewarping_params.h>

class QnResourceWidgetRenderer;

class QnVirtualPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnVirtualPtzController(const QnResourcePtr &resource): base_type(resource), m_animationEnabled(false) {}

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool animationEnabled) { m_animationEnabled = animationEnabled; }

    virtual void changePanoMode() = 0;
    virtual QString getPanoModeText() const = 0;

    virtual void setEnabled(bool value) = 0;
    virtual bool isEnabled() const = 0;

private:
    bool m_animationEnabled;
};

class QnFisheyePtzController: public QnVirtualPtzController {
    Q_OBJECT
public:
    QnFisheyePtzController(const QnMediaResourcePtr &resource);
    virtual ~QnFisheyePtzController();

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool getLimits(QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport) override;

    void addRenderer(QnResourceWidgetRenderer* renderer);
    //void setAspectRatio(float aspectRatio);
    DewarpingParams updateDewarpingParams(float ar);

    DewarpingParams getDewarpingParams() const;
    void setDewarpingParams(const DewarpingParams params);

    virtual void setEnabled(bool value) override;
    virtual bool isEnabled() const override;

    void moveToRect(const QRectF& r);

    virtual void changePanoMode() override;
    virtual QString getPanoModeText() const override;

signals:
    void dewarpingParamsChanged(DewarpingParams params);
    void spaceMapperChanged();

private slots:
    void at_timer();

private:
    qreal boundXAngle(qreal value, qreal fov) const;
    qreal boundYAngle(qreal value, qreal fov, qreal aspectRatio, DewarpingParams::ViewMode viewMode) const;
    void updateSpaceMapper(DewarpingParams::ViewMode viewMode, int pf);

private:
    QnMediaResourcePtr m_resource;
    QVector3D m_motion;
    QnResourceWidgetRenderer * m_renderer;
    DewarpingParams m_dewarpingParams;
    DewarpingParams m_srcPos;
    DewarpingParams m_dstPos;
    qint64 m_lastTime;
    bool m_moveToAnimation;
    //QnPtzSpaceMapper* m_spaceMapper;

    struct SpaceRange {
        SpaceRange(): min(0.0), max(0.0) {}
        SpaceRange(qreal _min, qreal _max): min(_min), max(_max) {}
        qreal min;
        qreal max;
    };
    SpaceRange m_xRange;
    SpaceRange m_yRange;
    qreal m_lastAR;
};

#endif // __FISHEYE_PTZ_CONTROLLER_H__

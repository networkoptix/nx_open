#ifndef __FISHEYE_PTZ_CONTROLLER_H__
#define __FISHEYE_PTZ_CONTROLLER_H__

#include <QVector3D>

#include <core/resource/interface/abstract_ptz_controller.h>
#include <core/resource/dewarping_params.h>

class QnResourceWidgetRenderer;

class QnFisheyePtzController: public QnVirtualPtzController {
    Q_OBJECT
public:
    QnFisheyePtzController(QnResource* resource);
    virtual ~QnFisheyePtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

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
    QVector3D m_motion;
    QnResourceWidgetRenderer* m_renderer;
    DewarpingParams m_dewarpingParams;
    DewarpingParams m_srcPos;
    DewarpingParams m_dstPos;
    qint64 m_lastTime;
    bool m_moveToAnimation;
    QnPtzSpaceMapper* m_spaceMapper;

    struct SpaceRange {
        SpaceRange(): min(0.0), max(0.0) {}
        SpaceRange(qreal _min, qreal _max): min(_min), max(_max) {}
        qreal min;
        qreal max;
    };
    SpaceRange m_xRange;
    SpaceRange m_yRange;
};

#endif // __FISHEYE_PTZ_CONTROLLER_H__

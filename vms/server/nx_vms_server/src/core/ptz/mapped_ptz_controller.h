#pragma once

#include <core/ptz/proxy_ptz_controller.h>

/**
 * A proxy ptz controller that uses a PTZ space mapper to provide absolute
 * PTZ commands in standard PTZ space.
 */
class QnMappedPtzController: public QnProxyPtzController {
    Q_OBJECT;
    typedef QnProxyPtzController base_type;

public:
    QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* position,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

private:
    QnPtzMapperPtr m_mapper;
    QnPtzLimits m_limits;
};

#pragma once

#include <core/ptz/proxy_ptz_controller.h>
#include <nx/core/ptz/realtive/relative_move_executor.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeMoveWorkaroundController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    RelativeMoveWorkaroundController(const QnPtzControllerPtr& controller);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(
        qreal direction,
        const nx::core::ptz::Options& options) override;

private:
    Ptz::Capability extendsWith(
        Ptz::Capability relativeMoveCapability,
        const nx::core::ptz::Options& options) const;

private:
    std::unique_ptr<RelativeMoveExecutor> m_continuousMoveExecutor;
    std::unique_ptr<RelativeMoveExecutor> m_absoluteMoveExecutor;
};

} // namespace ptz
} // namespace core
} // namespace nx

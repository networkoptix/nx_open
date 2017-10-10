#pragma once

#include <nx/utils/move_only_func.h>

namespace nx {
namespace cdb {

namespace conf { class Settings; }

class AbstractVmsGateway
{
public:
    virtual ~AbstractVmsGateway() = default;

    virtual void merge(
        const std::string& targetSystemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;
};

class VmsGateway:
    public AbstractVmsGateway
{
public:
    VmsGateway(const conf::Settings& settings);
    virtual ~VmsGateway() override = default;

    virtual void merge(
        const std::string& targetSystemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler) override;

private:
    const conf::Settings& m_settings;
};

} // namespace cdb
} // namespace nx

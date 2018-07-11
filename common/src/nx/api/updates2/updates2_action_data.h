#pragma once

#include <QtCore>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/vms/api/data/software_version.h>

namespace nx {
namespace api {

struct Updates2ActionData
{
    Q_GADGET

public:
    enum class ActionCode
    {
        download,
        stop,
        install,
        check
    };
    Q_ENUM(ActionCode)

    ActionCode action;
    nx::vms::api::SoftwareVersion targetVersion;
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Updates2ActionData::ActionCode)

#define Updates2ActionData_Fields (action)(targetVersion)
QN_FUSION_DECLARE_FUNCTIONS(Updates2ActionData::ActionCode, (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2ActionData), (json)(ubjson))

} // namespace api
} // namespace nx

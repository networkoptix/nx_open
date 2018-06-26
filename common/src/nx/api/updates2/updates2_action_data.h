#pragma once

#include <QtCore>

#include <utils/common/software_version.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

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
    QnSoftwareVersion targetVersion;
};

#define Updates2ActionData_Fields (action)(targetVersion)
QN_FUSION_DECLARE_FUNCTIONS(Updates2ActionData::ActionCode, (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2ActionData), (json))

} // namespace api
} // namespace nx

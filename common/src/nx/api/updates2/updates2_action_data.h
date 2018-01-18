#pragma once

#include <QtCore>

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
        install
    };
    Q_ENUM(ActionCode)

    ActionCode action;
};

#define Updates2ActionData_Fields (action)
QN_FUSION_DECLARE_FUNCTIONS(Updates2ActionData::ActionCode, (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2ActionData), (json))

} // namespace api
} // namespace nx

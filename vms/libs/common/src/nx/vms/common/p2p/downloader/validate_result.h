#pragma once

#include <QtCore/QObject>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

struct ValidateResult
{
    bool success;
    ValidateResult();
    explicit ValidateResult(bool success);
};

#define ValidateResult_Fields (success)
QN_FUSION_DECLARE_FUNCTIONS(ValidateResult, (json)(eq))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx

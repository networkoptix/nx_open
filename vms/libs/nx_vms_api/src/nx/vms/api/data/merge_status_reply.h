#pragma once

#include <nx/vms/api/data_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API MergeStatusReply
{
    QnUuid mergeId;
    bool mergeInProgress = false;
};
#define MergeStatusReply_Fields (mergeId)(mergeInProgress)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::MergeStatusReply)

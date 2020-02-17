#pragma once

#include <nx/vms/api/data_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API MergeStatusReply
{
    /**%apidoc Id of the last merge operation. */
    QnUuid mergeId;

    /**%apidoc Whether the last merge operation is in progress. */
    bool mergeInProgress = false;
};
#define MergeStatusReply_Fields (mergeId)(mergeInProgress)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::MergeStatusReply)

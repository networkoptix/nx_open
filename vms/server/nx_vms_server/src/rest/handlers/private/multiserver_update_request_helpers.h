#pragma once

#include <rest/helpers/request_context.h>
#include <nx/utils/move_only_func.h>
#include <nx/update/common_update_manager.h>
#include "multiserver_request_helper.h"

class QnCommonModule;
class QnEmptyRequestData;

namespace nx::update { struct Status; };

namespace detail {

void checkUpdateStatusRemotely(
    const IfParticipantPredicate& ifParticipantPredicate,
    QnCommonModule* commonModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context);

IfParticipantPredicate makeIfParticipantPredicate(nx::CommonUpdateManager* updateManager);

} // namespace detail
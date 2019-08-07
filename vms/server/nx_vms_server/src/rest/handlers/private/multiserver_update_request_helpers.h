#pragma once

#include <rest/helpers/request_context.h>
#include <nx/utils/move_only_func.h>
#include <nx/vms/server/update/update_manager.h>
#include "multiserver_request_helper.h"

class QnMediaServerModule;
class QnEmptyRequestData;

namespace nx::update { struct Status; };

namespace detail {

void checkUpdateStatusRemotely(
    const IfParticipantPredicate& ifParticipantPredicate,
    QnMediaServerModule* serverModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context);

IfParticipantPredicate makeIfParticipantPredicate(
    nx::vms::server::UpdateManager* updateManager, const QList<QnUuid>& forcedParticipants = {});

} // namespace detail

#pragma once

#include <rest/helpers/request_context.h>
#include <nx/utils/move_only_func.h>
#include <nx/update/common_update_manager.h>
#include <nx/update/update_storages_helper.h>
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

void getStoragesDataRemotely(
    const IfParticipantPredicate& ifParticipantPredicate,
    QnMediaServerModule* serverModule,
    const QString& path,
    nx::update::storage::ServerToStoragesList* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context);

IfParticipantPredicate makeIfParticipantPredicate(nx::CommonUpdateManager* updateManager);

} // namespace detail
// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/access_rights_data_deprecated.h>
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractUserNotificationManager: public QObject
{
    Q_OBJECT

signals:
    /**
     * Notice: should keep ec2:: namespace for all signal declarations, or moc/qt signals
     * will fail to pick correct metadata when you try to emit queued signal
     */
    void addedOrUpdated(const nx::vms::api::UserData& user, ec2::NotificationSource source);
    void userRoleAddedOrUpdated(const nx::vms::api::UserGroupData& userRole);
    void removed(const QnUuid& id, ec2::NotificationSource source);
    void userRoleRemoved(const QnUuid& id);
    void accessRightsChanged(const nx::vms::api::AccessRightsDataDeprecated& access);
};

/**
 * NOTE: All methods are asynchronous if not specified otherwise.
 */
class NX_VMS_COMMON_API AbstractUserManager
{
public:
    virtual ~AbstractUserManager() = default;

    virtual int getUsers(
        Handler<nx::vms::api::UserDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getUsersSync(nx::vms::api::UserDataList* outDataList);

    virtual int save(
        const nx::vms::api::UserData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result saveSync(const nx::vms::api::UserData& data);

    virtual int save(
        const nx::vms::api::UserDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::UserDataList& dataList);

    virtual int remove(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int remove(
        const nx::vms::api::IdDataList& ids,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const QnUuid& id);

    virtual int getUserRoles(
        Handler<nx::vms::api::UserGroupDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getUserRolesSync(nx::vms::api::UserGroupDataList* outDataList);

    virtual int saveUserRole(
        const nx::vms::api::UserGroupData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveUserRoleSync(const nx::vms::api::UserGroupData& data);

    virtual int removeUserRole(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int removeUserRoles(
        const nx::vms::api::IdDataList& ids,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeUserRoleSync(const QnUuid& id);
};

} // namespace ec2

#pragma once

#include <nx/clusterdb/engine/command_descriptor.h>

#include <transaction/transaction_descriptor.h>

namespace nx::cloud::db::ec2::command {

struct SaveUser:
    nx::clusterdb::engine::command::BaseCommandDescriptor<
        vms::api::UserData,
        ::ec2::ApiCommand::saveUser>
{
    static constexpr char name[] = "saveUser";
};

struct RemoveUser:
    nx::clusterdb::engine::command::BaseCommandDescriptor<
        vms::api::IdData,
        ::ec2::ApiCommand::removeUser>
{
    static constexpr char name[] = "removeUser";
};

struct SetResourceParam:
    nx::clusterdb::engine::command::BaseCommandDescriptor<
        vms::api::ResourceParamWithRefData,
        ::ec2::ApiCommand::setResourceParam>
{
    static constexpr char name[] = "setResourceParam";
};

struct RemoveResourceParam:
    nx::clusterdb::engine::command::BaseCommandDescriptor<
        vms::api::ResourceParamWithRefData,
        ::ec2::ApiCommand::removeResourceParam>
{
    static constexpr char name[] = "removeResourceParam";
};

struct SaveSystemMergeHistoryRecord:
    nx::clusterdb::engine::command::BaseCommandDescriptor<
        vms::api::SystemMergeHistoryRecord,
        ::ec2::ApiCommand::saveSystemMergeHistoryRecord>
{
    static constexpr char name[] = "saveSystemMergeHistoryRecord";
};

} // namespace nx::cloud::db::ec2::command

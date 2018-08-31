#pragma once

#include <transaction/transaction_descriptor.h>

namespace nx::cdb::ec2::command {

struct SaveUser
{
    using Data = vms::api::UserData;
    static constexpr int code = ::ec2::ApiCommand::saveUser;
    static constexpr char name[] = "saveUser";

    static nx::Buffer hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data).toSimpleByteArray();
    }
};

struct RemoveUser
{
    using Data = vms::api::IdData;
    static constexpr int code = ::ec2::ApiCommand::removeUser;
    static constexpr char name[] = "removeUser";

    static nx::Buffer hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data).toSimpleByteArray();
    }
};

struct SetResourceParam
{
    using Data = vms::api::ResourceParamWithRefData;
    static constexpr int code = ::ec2::ApiCommand::setResourceParam;
    static constexpr char name[] = "setResourceParam";

    static nx::Buffer hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data).toSimpleByteArray();
    }
};

struct RemoveResourceParam
{
    using Data = vms::api::ResourceParamWithRefData;
    static constexpr int code = ::ec2::ApiCommand::removeResourceParam;
    static constexpr char name[] = "removeResourceParam";

    static nx::Buffer hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data).toSimpleByteArray();
    }
};

struct SaveSystemMergeHistoryRecord
{
    using Data = vms::api::SystemMergeHistoryRecord;
    static constexpr int code = ::ec2::ApiCommand::saveSystemMergeHistoryRecord;
    static constexpr char name[] = "saveSystemMergeHistoryRecord";

    static nx::Buffer hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data).toSimpleByteArray();
    }
};

} // namespace nx::cdb::ec2::command

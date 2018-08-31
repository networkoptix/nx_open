#pragma once

#include <transaction/transaction_descriptor.h>

namespace nx::cdb::ec2::command {

struct SaveUser
{
    using Data = vms::api::UserData;
    static constexpr int code = ::ec2::ApiCommand::saveUser;
    static constexpr char name[] = "saveUser";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct RemoveUser
{
    using Data = vms::api::IdData;
    static constexpr int code = ::ec2::ApiCommand::removeUser;
    static constexpr char name[] = "removeUser";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct SetResourceParam
{
    using Data = vms::api::ResourceParamWithRefData;
    static constexpr int code = ::ec2::ApiCommand::setResourceParam;
    static constexpr char name[] = "setResourceParam";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct RemoveResourceParam
{
    using Data = vms::api::ResourceParamWithRefData;
    static constexpr int code = ::ec2::ApiCommand::removeResourceParam;
    static constexpr char name[] = "removeResourceParam";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct SaveSystemMergeHistoryRecord
{
    using Data = vms::api::SystemMergeHistoryRecord;
    static constexpr int code = ::ec2::ApiCommand::saveSystemMergeHistoryRecord;
    static constexpr char name[] = "saveSystemMergeHistoryRecord";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

} // namespace nx::cdb::ec2::command

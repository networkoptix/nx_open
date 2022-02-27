// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>

namespace nx::vms::api {

struct UserDataEx;
struct StorageData;
struct ResourceParamData;
struct ResourceParamWithRefData;
struct EventRuleData;

} // namespace nx::vms::api

namespace ec2 {

template<typename T>
auto fixTransactionInputFromApi(const QnTransaction<T>& tran, Result* result)
{
    *result = Result();
    return tran;
}

QnTransaction<nx::vms::api::UserData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::UserDataEx>& originalTran, Result* result);

QnTransaction<nx::vms::api::StorageData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::StorageData>& originalTran, Result* result);

QnTransaction<nx::vms::api::ResourceParamData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamData>& originalTran, Result* result);

QnTransaction<nx::vms::api::ResourceParamWithRefData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamWithRefData>& originalTran, Result* result);

QnTransaction<nx::vms::api::EventRuleData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::EventRuleData>& originalTran, Result* result);

} // namespace ec2

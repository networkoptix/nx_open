// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>

namespace nx::vms::api {

struct UserDataEx;
struct StorageData;
struct ResourceParamData;
struct ResourceParamWithRefData;
struct EventRuleData;
struct LayoutData;

namespace rules { struct Rule; }

} // namespace nx::vms::api

namespace ec2 {

template<typename RequestData>
inline Result fixRequestDataIfNeeded(RequestData*) { return {}; }

Result fixRequestDataIfNeeded(nx::vms::api::UserDataEx* userDataEx);
Result fixRequestDataIfNeeded(nx::vms::api::ResourceParamData* paramData);
Result fixRequestDataIfNeeded(nx::vms::api::StorageData* paramData);
Result fixRequestDataIfNeeded(nx::vms::api::LayoutData* paramData);
Result fixRequestDataIfNeeded(nx::vms::api::EventRuleData* paramData);
Result fixRequestDataIfNeeded(nx::vms::api::rules::Rule* paramData);

template<typename T>
auto fixTransactionInputFromApi(const QnTransaction<T>& original, Result* result)
{
    auto fixedTransaction = original;
    *result = fixRequestDataIfNeeded(&fixedTransaction.params);
    return fixedTransaction;
}

QnTransaction<nx::vms::api::UserData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::UserDataEx>& originalTran, Result* result);

QnTransaction<nx::vms::api::ResourceParamWithRefData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamWithRefData>& originalTran, Result* result);

} // namespace ec2

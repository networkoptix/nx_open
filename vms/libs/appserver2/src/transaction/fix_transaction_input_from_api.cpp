// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fix_transaction_input_from_api.h"

#include <nx/fusion/serialization/json.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/event/action_parameters.h>

#include <api/model/password_data.h>
#include <utils/crypt/symmetrical.h>
#include <transaction/transaction_descriptor.h>

namespace ec2 {

extern const std::set<QString> kResourceParamToAmend;

namespace {

template<typename RequestData>
inline Result fixRequestDataIfNeeded(RequestData* /*requestData*/)
{
    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::UserDataEx* userDataEx)
{
    if (!userDataEx->password.isEmpty())
    {
        const auto hashes = PasswordData::calculateHashes(userDataEx->name, userDataEx->password,
            userDataEx->isLdap);

        userDataEx->realm = hashes.realm;
        userDataEx->hash = hashes.passwordHash;
        userDataEx->cryptSha512Hash = hashes.cryptSha512Hash;
        if (userDataEx->digest != nx::vms::api::UserData::kHttpIsDisabledStub)
            userDataEx->digest = hashes.passwordDigest;
    }
    if (userDataEx->realm.isEmpty())
        userDataEx->realm = QString::fromStdString(nx::network::AppInfo::realm());

    if (userDataEx->isCloud)
    {
        if (userDataEx->name.isEmpty())
            userDataEx->name = userDataEx->email;

        if (userDataEx->digest.isEmpty())
            userDataEx->digest = nx::vms::api::UserData::kCloudPasswordStub;
    }
    if (userDataEx->adaptFromDeprecatedApi())
        return Result();
    return Result(ErrorCode::badRequest, "Conflict in Deprecated API fields");
}

Result fixRequestDataIfNeeded(nx::vms::api::ResourceParamData* paramData)
{
    if (kResourceParamToAmend.contains(paramData->name))
        paramData->value = nx::utils::encodeHexStringFromStringAES128CBC(paramData->value);
    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::StorageData* paramData)
{
    nx::utils::Url url = paramData->url;
    if (url.password().isEmpty())
        return Result();

    url.setPassword(nx::utils::encodeHexStringFromStringAES128CBC(url.password()));
    paramData->url = url.toString();
    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::LayoutData* paramData)
{
    for (auto& item: paramData->items)
    {
        if (item.id.isNull())
            item.id = QnUuid::createUuid();
    }
    return Result();
}

template<typename Data>
auto doFix(const QnTransaction<Data>& original, Result* result)
{
    auto fixedTransaction = original;
    *result = fixRequestDataIfNeeded(&fixedTransaction.params);

    return fixedTransaction;
}

} // namespace

QnTransaction<nx::vms::api::UserData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::UserDataEx>& originalTran, Result* result)
{
    auto originalParams = originalTran.params;
    *result = fixRequestDataIfNeeded(&originalParams);

    QnTransaction<nx::vms::api::UserData> resultTran(
        static_cast<const QnAbstractTransaction&>(originalTran));
    resultTran.params = nx::vms::api::UserData(originalParams);
    return resultTran;
}

QnTransaction<nx::vms::api::StorageData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::StorageData>& originalTran, Result* result)
{
    return doFix(originalTran, result);
}

QnTransaction<nx::vms::api::ResourceParamData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamData>& originalTran, Result* result)
{
    return doFix(originalTran, result);
}

QnTransaction<nx::vms::api::ResourceParamWithRefData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamWithRefData>& originalTran, Result* result)
{
    auto resultTran = originalTran;
    *result = fixRequestDataIfNeeded(static_cast<nx::vms::api::ResourceParamData*>(&resultTran.params));

    return resultTran;
}

QnTransaction<nx::vms::api::EventRuleData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::EventRuleData>& originalTran, Result* result)
{
    nx::vms::event::ActionParameters params;
    if (!QJson::deserialize(originalTran.params.actionParams, &params))
    {
        *result = Result(ErrorCode::badRequest,
            NX_FMT("Failed to deserialize `actionParams`: %1", originalTran.params.actionParams));
        return originalTran;
    }

    if (!params.httpMethod.isEmpty()
        && !nx::network::http::Method::isKnown(params.httpMethod.toStdString()))
    {
        *result = Result(ErrorCode::badRequest,
            NX_FMT("Unsupported `actionParams.httpMethod`: %1", params.httpMethod));
        return originalTran;
    }

    *result = Result();
    if (nx::utils::Url url = params.url; !url.password().isEmpty())
    {
        url.setPassword(nx::utils::encodeHexStringFromStringAES128CBC(url.password()));
        params.url = url.toString();
        QnTransaction<nx::vms::api::EventRuleData> fixedTransaction(originalTran);
        fixedTransaction.params.actionParams = QJson::serialized(params);
        return fixedTransaction;
    }

    return originalTran;
}

QnTransaction<nx::vms::api::LayoutData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::LayoutData>& originalTran, Result* result)
{
    return doFix(originalTran, result);
}

} // namespace ec2

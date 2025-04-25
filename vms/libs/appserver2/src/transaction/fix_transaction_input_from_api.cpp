// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fix_transaction_input_from_api.h"

#include <api/model/password_data.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/rules/encryption.h>
#include <transaction/amend_transaction_data.h>
#include <transaction/transaction_descriptor.h>

namespace ec2 {

Result fixRequestDataIfNeeded(nx::vms::api::UserDataEx* userDataEx)
{
    if (userDataEx->type == nx::vms::api::UserType::temporaryLocal)
    {
        NX_ASSERT(!userDataEx->hash.isEmpty());
        return Result(); //< Everything should be filled already.
    }

    if (!userDataEx->password.isEmpty())
    {
        const auto hashes = PasswordData::calculateHashes(
            userDataEx->name, userDataEx->password,
            userDataEx->type == nx::vms::api::UserType::ldap);

        userDataEx->hash = hashes.passwordHash;
        userDataEx->cryptSha512Hash = hashes.cryptSha512Hash;
        if (userDataEx->digest != nx::vms::api::UserData::kHttpIsDisabledStub)
            userDataEx->digest = hashes.passwordDigest;
    }

    if (userDataEx->type == nx::vms::api::UserType::cloud)
    {
        if (userDataEx->name.isEmpty())
            userDataEx->name = userDataEx->email;

        if (userDataEx->digest.isEmpty())
            userDataEx->digest = nx::vms::api::UserData::kCloudPasswordStub;
    }

    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::ResourceParamData* paramData)
{
    if (resourceParamToAmend().contains(paramData->name))
        paramData->value = nx::crypt::encodeHexStringFromStringAES128CBC(paramData->value);

    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::StorageData* paramData)
{
    nx::utils::Url url = paramData->url;
    if (url.password().isEmpty())
        return Result();

    url.setPassword(nx::crypt::encodeHexStringFromStringAES128CBC(url.password()));
    paramData->url = url.toString();
    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::LayoutData* paramData)
{
    for (auto& item: paramData->items)
    {
        if (item.id.isNull())
            item.id = nx::Uuid::createUuid();
    }
    return Result();
}

Result fixRequestDataIfNeeded(nx::vms::api::EventRuleData* paramData)
{
    nx::vms::event::ActionParameters params;
    if (!QJson::deserialize(paramData->actionParams, &params))
    {
        return Result(ErrorCode::badRequest,
            NX_FMT("Failed to deserialize `actionParams`: %1", paramData->actionParams));
    }

    if (!params.httpMethod.isEmpty()
        && !nx::network::http::Method::isKnown(params.httpMethod.toStdString()))
    {
        return Result(ErrorCode::badRequest,
            NX_FMT("Unsupported `actionParams.httpMethod`: %1", params.httpMethod));
    }

    if (nx::utils::Url url = params.url; !url.password().isEmpty())
    {
        url.setPassword(nx::crypt::encodeHexStringFromStringAES128CBC(url.password()));
        params.url = url.toString();
        paramData->actionParams = QJson::serialized(params);
    }

    return {};
}

Result fixRequestDataIfNeeded(nx::vms::api::rules::Rule* paramData)
{
    for (auto& builder: paramData->actionList)
    {
        for (auto& [_, field]: builder.fields)
        {
            for (const auto& name: nx::vms::rules::encryptedActionBuilderProperties(field.type))
            {
                if (auto it = field.props.find(name); it != field.props.end())
                {
                    it.value() =
                        nx::crypt::encodeHexStringFromStringAES128CBC(it.value().toString());
                }
            }
        }
    }
    return {};
}

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

QnTransaction<nx::vms::api::ResourceParamWithRefData> fixTransactionInputFromApi(
    const QnTransaction<nx::vms::api::ResourceParamWithRefData>& originalTran, Result* result)
{
    auto resultTran = originalTran;
    *result = fixRequestDataIfNeeded(static_cast<nx::vms::api::ResourceParamData*>(&resultTran.params));
    return resultTran;
}

} // namespace ec2

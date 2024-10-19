// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_ipc_data.h"

#include "jni_helpers.h"

namespace {

static constexpr auto kStorageClass = "com/nxvms/mobile/utils/PushIpcData";

} // namespace

namespace nx::vms::client::mobile::details {

void PushIpcData::store(
    const std::string& user,
    const std::string& cloudRefreshToken,
    const std::string& password)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return;

    PushIpcData::clear();

    // Signature for "void store(Context, String, String, String)" function.
    static const auto kStoreFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kString, JniSignature::kString});

    const auto userObject = QJniObject::fromString(user.c_str());
    const auto cloudRefreshTokenObject = QJniObject::fromString(cloudRefreshToken.c_str());
    const auto passwordObject = QJniObject::fromString(password.c_str());
    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "store",
        kStoreFunctionSignature.c_str(),
        activity.object(),
        userObject.object<jstring>(),
        cloudRefreshTokenObject.object<jstring>(),
        passwordObject.object<jstring>());
}

bool PushIpcData::load(
    std::string& user,
    std::string& cloudRefreshToken,
    std::string& password)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return false;

    static const auto kLoginInfoClassSignature =
        QString("Lcom/nxvms/mobile/utils/PushIpcData$LoginInfo;");

    // Signture for "PushApiData.LoginInfo load(Context)" function.
    static const auto kLoadFunctionSignature =
        makeSignature({JniSignature::kContext}, kLoginInfoClassSignature);

    const QJniObject object =
        QJniObject::callStaticObjectMethod(
            kStorageClass,
            "load",
            kLoadFunctionSignature.c_str(),
            activity.object());

    if (!object.isValid())
        return false;

    user = object.getObjectField(
        "user", JniSignature::kString).toString().toStdString();
    cloudRefreshToken = object.getObjectField(
        "cloudRefreshToken", JniSignature::kString).toString().toStdString();
    password = object.getObjectField(
        "password", JniSignature::kString).toString().toStdString();
    return true;
}

void PushIpcData::clear()
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return;

    // Signture for "void clear(String)" function.
    static const auto kClearFunctionSignature =
        makeVoidSignature({JniSignature::kContext});

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "clear",
        kClearFunctionSignature.c_str(),
        activity.object());
}

PushIpcData::TokenInfo PushIpcData::accessToken(
    const std::string& cloudSystemId)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return {};

    static const auto kTokenInfoClassSignature =
        QString("Lcom/nxvms/mobile/utils/PushIpcData$TokenInfo;");

    // Signature for "TokenInfo accessToken(Context, String)" function.
    static const auto kAccessTokenSignature =
        makeSignature({JniSignature::kContext, JniSignature::kString}, kTokenInfoClassSignature);

    const auto cloudSystemIdObject = QJniObject::fromString(cloudSystemId.c_str());

    const QJniObject object =
        QJniObject::callStaticObjectMethod(
            kStorageClass,
            "accessToken",
            kAccessTokenSignature.c_str(),
            activity.object(),
            cloudSystemIdObject.object<jstring>());

    if (!object.isValid())
        return {};

    return {
        .accessToken = object.getObjectField(
            "accessToken", JniSignature::kString).toString().toStdString(),
        .expiresAt = std::chrono::milliseconds(object.getField<jlong>("expiresAt"))};
}

void PushIpcData::setAccessToken(
    const std::string& cloudSystemId,
    const std::string& accessToken,
    const std::chrono::milliseconds& expiresAt)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return;

    // Signature for "void setAccessToken(Context, String, String, long)" function.
    static const auto kSetAccessTokenFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kString, JniSignature::kLong});

    const auto cloudSystemIdObject = QJniObject::fromString(cloudSystemId.c_str());
    const auto accessTokenObject = QJniObject::fromString(accessToken.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "setAccessToken",
        kSetAccessTokenFunctionSignature.c_str(),
        activity.object(),
        cloudSystemIdObject.object<jstring>(),
        accessTokenObject.object<jstring>(),
        static_cast<jlong>(expiresAt.count()));
}

void PushIpcData::removeAccessToken(const std::string& cloudSystemId)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return;

    // Signature for "void removeAccessToken(Context, String)" function.
    static const auto kRemoveAccessTokenFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString});

    const auto cloudSystemIdObject = QJniObject::fromString(cloudSystemId.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "removeAccessToken",
        kRemoveAccessTokenFunctionSignature.c_str(),
        activity.object(),
        cloudSystemIdObject.object<jstring>());
}

bool PushIpcData::cloudLoggerOptions(
    std::string& logSessionId,
    std::chrono::milliseconds& sessionEndTime)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return false;

    // Signture for "CloudLoggerOptions cloudLoggerOption(Context)" function.
    static const auto kLoadFunctionSignature =
        makeSignature({JniSignature::kContext},
            "Lcom/nxvms/mobile/utils/CloudLoggerOptions;");

    const QJniObject object =
        QJniObject::callStaticObjectMethod(
            kStorageClass,
            "cloudLoggerOptions",
            kLoadFunctionSignature.c_str(),
            activity.object());

    if (!object.isValid())
        return false;

    logSessionId = object.getObjectField(
        "logSessionId", JniSignature::kString).toString().toStdString();
    sessionEndTime = std::chrono::milliseconds(object.getField<jlong>("sessionEndTimeMs"));
    return true;
}

void PushIpcData::setCloudLoggerOptions(
    const std::string& logSessionId,
    const std::chrono::milliseconds& sessionEndTime)
{
    const auto activity = currentActivity();
    if (!activity.isValid())
        return;

    // Signature for "void setCloudLoggerOptions(Context, String, String)" function.
    static const auto kStoreFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kLong});

    const auto logSessionIdObject = QJniObject::fromString(logSessionId.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "setCloudLoggerOptions",
        kStoreFunctionSignature.c_str(),
        activity.object(),
        logSessionIdObject.object<jstring>(),
        static_cast<jlong>(sessionEndTime.count()));
}

} // namespace nx::vms::client::mobile::details

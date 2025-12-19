// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "android_secure_storage.h"

#include <string>

#include <QtCore/QString>

#include "../utils.h"
#include "jni_helpers.h"

namespace nx::vms::client::mobile::details {

namespace {

constexpr auto kStorageClass = "com/nxvms/mobile/utils/SecureStorage";

} // namespace

AndroidSecureStorage::AndroidSecureStorage(const QJniObject& context):
    m_context(context.isValid() ? context : currentActivity())
{
}

std::optional<std::string> AndroidSecureStorage::load(const std::string& key) const
{
    if (!m_context.isValid())
        return std::nullopt;

    static const auto kFunctionSignature = makeSignature(
        {JniSignature::kContext, JniSignature::kString}, JniSignature::kString);

    const auto keyObject = QJniObject::fromString(key.c_str());
    const QJniObject result = QJniObject::callStaticObjectMethod(
        kStorageClass,
        "load",
        kFunctionSignature.c_str(),
        m_context.object(),
        keyObject.object<jstring>());

    if (!result.isValid())
        return std::nullopt;

    return result.toString().toStdString();
}

void AndroidSecureStorage::save(const std::string& key, const std::string& value)
{
    if (!m_context.isValid())
        return;

    static const auto kFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kString});

    const auto keyObject = QJniObject::fromString(key.c_str());
    const auto valueObject = QJniObject::fromString(value.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "save",
        kFunctionSignature.c_str(),
        m_context.object(),
        keyObject.object<jstring>(),
        valueObject.object<jstring>());
}

std::optional<std::vector<std::byte>> AndroidSecureStorage::loadFile(const std::string& key) const
{
    if (!m_context.isValid())
        return std::nullopt;

    static const auto kFunctionSignature = makeSignature(
        {JniSignature::kContext, JniSignature::kString}, JniSignature::kByteArray);

    const auto keyObject = QJniObject::fromString(QString::number(hash(key)));
    const QJniObject dataObject = QJniObject::callStaticObjectMethod(
        kStorageClass,
        "loadFile",
        kFunctionSignature.c_str(),
        m_context.object(),
        keyObject.object<jstring>());

    if (!dataObject.isValid())
        return std::nullopt;

    QJniEnvironment env;
    jbyteArray data = dataObject.object<jbyteArray>();
    jsize size = env->GetArrayLength(data);

    std::vector<std::byte> result(size);
    env->GetByteArrayRegion(data, 0, result.size(), reinterpret_cast<jbyte*>(result.data()));

    return result;
}

void AndroidSecureStorage::saveFile(const std::string& key, const std::vector<std::byte>& data)
{
    if (!m_context.isValid())
        return;

    static const auto kFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kByteArray});

    const auto keyObject = QJniObject::fromString(QString::number(hash(key)));

    QJniEnvironment env;
    const auto dataObject = QJniObject::fromLocalRef(env->NewByteArray(data.size()));
    env->SetByteArrayRegion(
        dataObject.object<jbyteArray>(),
        0,
        data.size(),
        reinterpret_cast<const jbyte*>(data.data()));

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "saveFile",
        kFunctionSignature.c_str(),
        m_context.object(),
        keyObject.object<jstring>(),
        dataObject.object<jbyteArray>());
}

void AndroidSecureStorage::removeFile(const std::string& key)
{
    if (!m_context.isValid())
        return;

    static const auto kFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString});

    const auto keyObject = QJniObject::fromString(QString::number(hash(key)));

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "removeFile",
        kFunctionSignature.c_str(),
        m_context.object(),
        keyObject.object<jstring>());
}

} // namespace nx::vms::client::mobile::details

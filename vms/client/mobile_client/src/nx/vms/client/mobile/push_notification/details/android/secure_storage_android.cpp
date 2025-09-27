// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../secure_storage.h"

#include <QtCore/QString>

#include <string>

#include "jni_helpers.h"

namespace nx::vms::client::mobile {

using namespace details;

namespace {

constexpr auto kStorageClass = "com/nxvms/mobile/utils/SecureStorage";

} // namespace

template<>
SecureStorage::SecureStorage(const QJniObject& context):
    m_context(context.isValid() ? context : currentActivity())
{
}

std::optional<std::string> SecureStorage::load(const std::string& key) const
{
    const auto context = std::any_cast<QJniObject>(&m_context);
    if (!context || !context->isValid())
        return std::nullopt;

    static const auto kFunctionSignature = makeSignature(
        {JniSignature::kContext, JniSignature::kString}, JniSignature::kString);

    const auto keyObject = QJniObject::fromString(key.c_str());
    const QJniObject result = QJniObject::callStaticObjectMethod(
        kStorageClass,
        "load",
        kFunctionSignature.c_str(),
        context->object(),
        keyObject.object<jstring>());

    if (!result.isValid())
        return std::nullopt;

    return result.toString().toStdString();
}

void SecureStorage::save(const std::string& key, const std::string& value)
{
    const auto context = std::any_cast<QJniObject>(&m_context);
    if (!context || !context->isValid())
        return;

    static const auto kFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString, JniSignature::kString});

    const auto keyObject = QJniObject::fromString(key.c_str());
    const auto valueObject = QJniObject::fromString(value.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "save",
        kFunctionSignature.c_str(),
        context->object(),
        keyObject.object<jstring>(),
        valueObject.object<jstring>());
}

std::optional<std::vector<std::byte>> SecureStorage::loadImage(const std::string& id) const
{
    const auto context = std::any_cast<QJniObject>(&m_context);
    if (!context || !context->isValid())
        return std::nullopt;

    static const auto kFunctionSignature = makeSignature(
        {JniSignature::kContext, JniSignature::kString}, JniSignature::kByteArray);

    const auto pathObject = QJniObject::fromString(id.c_str());
    const QJniObject dataObject = QJniObject::callStaticObjectMethod(
        kStorageClass,
        "loadImage",
        kFunctionSignature.c_str(),
        context->object(),
        pathObject.object<jstring>());

    if (!dataObject.isValid())
        return std::nullopt;

    QJniEnvironment env;
    jbyteArray data = dataObject.object<jbyteArray>();
    jsize size = env->GetArrayLength(data);

    std::vector<std::byte> result(size);
    env->GetByteArrayRegion(data, 0, result.size(), reinterpret_cast<jbyte*>(result.data()));

    return result;
}

void SecureStorage::removeImage(const std::string& id)
{
    const auto context = std::any_cast<QJniObject>(&m_context);
    if (!context || !context->isValid())
        return;

    static const auto kFunctionSignature = makeVoidSignature(
        {JniSignature::kContext, JniSignature::kString});

    const auto pathObject = QJniObject::fromString(id.c_str());

    QJniObject::callStaticMethod<void>(
        kStorageClass,
        "removeImage",
        kFunctionSignature.c_str(),
        context->object(),
        pathObject.object<jstring>());
}

} // namespace nx::vms::client::mobile

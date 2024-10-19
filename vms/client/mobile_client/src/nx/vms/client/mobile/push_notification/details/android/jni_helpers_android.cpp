// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jni_helpers.h"

namespace nx::vms::client::mobile::details {

QJniObject currentActivity()
{
    static constexpr auto kActivitySignature = "Landroid/app/Activity;";
    return QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        makeSignature({}, kActivitySignature).c_str());
}

QJniObject applicationContext()
{
    const auto activity = currentActivity();
    return activity.callObjectMethod(
        "getApplicationContext",
        makeSignature({}, JniSignature::kContext).c_str());
}

std::string makeSignature(
    const QStringList& arguments,
    const QString& result)
{
    return QString("(%1)%2").arg(arguments.join(""), result).toStdString();
}

std::string makeVoidSignature(const QStringList& arguments)
{
    static constexpr auto kVoidResultSignature = "V";
    return makeSignature(arguments, kVoidResultSignature);
}

std::string makeNoParamsVoidSignature()
{
    return makeVoidSignature({});
}

const char* JniSignature::kContext = "Landroid/content/Context;";
const char* JniSignature::kString = "Ljava/lang/String;";
const char* JniSignature::kLong = "J";

} // namespace nx::vms::client::mobile::details

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_notification_storage.h"

#include "android_secure_storage.h"
#include "jni_helpers.h"

namespace {

std::string toStr(const jstring& str)
{
    return QJniObject(str).toString().toStdString();
};

std::vector<std::byte> toBytes(JNIEnv* env, const jbyteArray& data)
{
    jsize size = env->GetArrayLength(data);
    std::vector<std::byte> result(size);
    env->GetByteArrayRegion(data, 0, result.size(), reinterpret_cast<jbyte*>(result.data()));
    return result;
}

} // namespace.

extern "C" {

using namespace nx::vms::client::mobile;

// Implementation of PushNotificationStorage.java.
JNIEXPORT jstring JNICALL Java_com_nxvms_mobile_utils_PushNotificationStorage_addUserNotification(
    JNIEnv* env,
    jclass,
    jobject context,
    jstring user,
    jstring title,
    jstring description,
    jstring url,
    jstring cloudSystemId,
    jstring imageUrl)
{
    auto secureStorage = std::make_shared<details::AndroidSecureStorage>(context);
    PushNotificationStorage notificationStorage{secureStorage};
    const std::string& id = notificationStorage.addUserNotification(
        toStr(user),
        toStr(title),
        toStr(description),
        toStr(url),
        toStr(cloudSystemId),
        toStr(imageUrl)
    );

    return env->NewStringUTF(id.c_str());
}

JNIEXPORT void JNICALL Java_com_nxvms_mobile_utils_PushNotificationStorage_saveImage(
    JNIEnv* env,
    jclass,
    jobject context,
    jstring url,
    jbyteArray image)
{
    auto secureStorage = std::make_shared<details::AndroidSecureStorage>(context);
    PushNotificationStorage notificationStorage{secureStorage};
    notificationStorage.saveImage(toStr(url), toBytes(env, image));
}

}

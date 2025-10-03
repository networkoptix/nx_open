// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_notification_storage.h"

#include "jni_helpers.h"

extern "C" {

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
    jstring imageId)
{
    using namespace nx::vms::client::mobile;
    auto toStr = [](const jstring& str) { return QJniObject(str).toString().toStdString(); };

    auto secureStorage = std::make_shared<SecureStorage>(QJniObject{context});
    PushNotificationStorage notificationStorage{secureStorage};
    const std::string& id = notificationStorage.addUserNotification(
        toStr(user),
        toStr(title),
        toStr(description),
        toStr(url),
        toStr(cloudSystemId),
        toStr(imageId)
    );

    return env->NewStringUTF(id.c_str());
}

}

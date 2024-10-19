// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../../token_data_watcher.h"

#include <QtCore/QJniObject>
#include <QtCore/QJniEnvironment>

namespace {

using namespace nx::vms::client::mobile::details;

void storeTokenData(
    JNIEnv* /*env*/,
    jobject /*this*/,
    jstring channelIdValue,
    jstring userIdValue)
{
    using namespace nx::vms::client::mobile::details;
    const auto channelId = QJniObject(channelIdValue).toString();
    const auto userId = QJniObject(userIdValue).toString();
    TokenDataWatcher::instance().setData(TokenData::makeBaidu(channelId, userId));
}

void resetTokenData(
    JNIEnv* /*env*/,
    jobject /*this*/)
{
    TokenDataWatcher::instance().resetData();
}

static const bool kRegistered =
    []()
    {
        static JNINativeMethod methods[]
        {
            {
                "storeTokenData",
                "(Ljava/lang/String;Ljava/lang/String;)V",
                reinterpret_cast<void*>(storeTokenData)
            },

            {
                "resetTokenData",
                "()V",
                reinterpret_cast<void*>(resetTokenData)
            },

        };

        QJniObject helperClass("com/nxvms/mobile/push/baidu/BaiduHelper");
        QJniEnvironment env;
        jclass objectClass = env->GetObjectClass(helperClass.object<jobject>());
        env->RegisterNatives(objectClass, methods, sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(objectClass);
        return true;
    }();

} // namespace

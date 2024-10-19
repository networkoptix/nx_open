// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <mutex>
#include <unordered_map>

#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/private/qjnihelpers_p.h>

#include <nx/utils/log/log_main.h>
#include <nx/utils/move_only_func.h>
#include <utils/common/delayed.h>

#include "../../token_data_provider.h"
#include "../../token_data_watcher.h"

static bool ensureNativeMethods();

namespace {

static constexpr auto kStorageClass = "com/nxvms/mobile/push/firebase/FirebaseTokenHelper";

using TokenCallback = nx::utils::MoveOnlyFunc<void(bool, const QString&)>;

class CallbackManager
{
public:
    jint put(TokenCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        const jint result = m_id++;
        if (result == std::numeric_limits<jint>::max())
            m_id = 0;
        m_callbacks.insert({result, std::move(callback)});
        return result;
    }

    void call(jint id, bool success, const QString& result)
    {
        TokenCallback callback;

        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            const auto it = m_callbacks.find(id);
            if (it == m_callbacks.end())
                return;
            callback = std::move(it->second);
            m_callbacks.erase(it);
        }

        callback(success, result);
    }

    static CallbackManager& instance()
    {
        static CallbackManager manager;
        return manager;
    }

private:
    std::unordered_map<jint, TokenCallback> m_callbacks;
    std::mutex m_callbackMutex;
    jint m_id = 0;
};

using namespace nx::vms::client::mobile::details;

static bool makeFirebaseRequest(
    std::function<void(jint)> request,
    TokenCallback callback,
    int triesCount,
    std::string logMarker)
{
    if (!ensureNativeMethods())
    {
        NX_ERROR(NX_SCOPE_TAG, "Unable to register JNI methods");
        return false;
    }

    auto internalCallback =
        [request, callback=std::move(callback), triesCount, logMarker](
            bool success, const QString& token) mutable
        {
            if (success)
            {
                callback(success, token);
                return;
            }

            if (triesCount > 1)
            {
                makeFirebaseRequest(request, std::move(callback), triesCount - 1, logMarker);
                return;
            }

            callback(success, token);
        };

    const auto id = CallbackManager::instance().put(std::move(internalCallback));

    request(id);

    return true;
}

} // namespace

static void onComplete(JNIEnv*, jclass, jint callbackId, jboolean success, jstring value)
{
    const QString token = QJniObject(value).toString();
    CallbackManager::instance().call(callbackId, success, token);
}
Q_DECLARE_JNI_NATIVE_METHOD(onComplete)

static bool ensureNativeMethods()
{
    static const bool ok = QJniEnvironment().registerNativeMethods(
        kStorageClass,
        {
            Q_JNI_NATIVE_METHOD(onComplete)
        });
    return ok;
}

namespace nx::vms::client::mobile::details {

TokenProviderType TokenDataProvider::type() const
{
    return TokenProviderType::firebase;
}

bool TokenDataProvider::requestTokenDataUpdate()
{
    auto request =
        [](jint callbackId)
        {
            QJniObject::callStaticMethod<void>(
                kStorageClass,
                "getToken",
                "(I)V",
                callbackId);
        };

    auto callback = threadGuarded(
        [](const TokenData& data)
        {
            TokenDataWatcher::instance().setData(data);
        });

    auto requestCallback =
        [callback=std::move(callback)](bool success, const QString& token)
        {
            callback(success
                ? TokenData::makeFirebase(token)
                : TokenData::makeEmpty());
        };

    return makeFirebaseRequest(
        std::move(request), std::move(requestCallback), /*triesCount*/ 3, "GET TOKEN");
}

bool TokenDataProvider::requestTokenDataReset()
{
    const auto request =
        [](jint callbackId)
        {
            QJniObject::callStaticMethod<void>(
                kStorageClass,
                "deleteToken",
                "(I)V",
                callbackId);
        };

    const auto requestCallback = threadGuarded(
        [](bool /*success*/, const QString& /*token*/)
        {
            TokenDataWatcher::instance().resetData();
        });

    return makeFirebaseRequest(
        std::move(request), std::move(requestCallback), /*triesCount*/ 3, "DELETE TOKEN");
}

} // namespace nx::vms::client::mobile::details

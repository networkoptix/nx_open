// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_controller.h"

#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <utils/common/delayed.h>

using AudioEnabled = bool;
using AudioDataHash = QHash<nx::Uuid, AudioEnabled>;
using UserDataHash = QHash<QString, AudioDataHash>;
using SystemDataHash = QHash<nx::Uuid, UserDataHash>;

namespace nx::vms::client::mobile {

struct AudioController::Private
{
    Private();

    QPointer<SessionManager> sessionManager;
    QString currentUser;
    nx::Uuid localSystemId;
    QnResourcePtr resource;
};

AudioController::Private::Private()
{
}

//--------------------------------------------------------------------------------------------------

void AudioController::registerQmlType()
{
    qmlRegisterType<AudioController>("Nx.Mobile", 1, 0, "AudioController");
}

AudioController::AudioController(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &AudioController::resourceChanged, this, &AudioController::audioEnabledChanged);
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this](int id)
        {
            if (id == QnMobileClientSettings::AudioSettings)
                emit audioEnabledChanged();
        });
}

AudioController::~AudioController()
{
}

void AudioController::setServerSessionManager(SessionManager* value)
{
    if (value == d->sessionManager)
        return;

    if (d->sessionManager)
        d->sessionManager->disconnect(this);

    d->sessionManager = value;

    if (!d->sessionManager)
        return;

    const auto updateSessionData =
        [this, value]()
    {
        if (d->currentUser == value->currentUser() && d->localSystemId == value->localSystemId())
            return;

        d->currentUser = d->sessionManager->currentUser();
        d->localSystemId = d->sessionManager->localSystemId();
        emit audioEnabledChanged();
    };

    connect(d->sessionManager, &SessionManager::sessionParametersChanged, this, updateSessionData);
    updateSessionData();
}

SessionManager* AudioController::serverSessionManager() const
{
    return d->sessionManager;
}

void AudioController::setRawResource(const QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

QnResource* AudioController::rawResource() const
{
    return core::withCppOwnership(d->resource);
}

void AudioController::setResource(const QnResourcePtr& value)
{
    if (value == d->resource)
        return;

    d->resource = value;
    emit resourceChanged();
}

QnResourcePtr AudioController::resource()
{
    return d->resource;
}

void AudioController::setAudioEnabled(bool value)
{
    if (audioEnabled() == value || !d->resource)
        return;

    auto data = QJson::deserialized<SystemDataHash>(qnSettings->audioSettings());
    data[d->localSystemId][d->currentUser][d->resource->getId()] = value;
    qnSettings->setAudioSettings(QJson::serialized(data));

    emit audioEnabledChanged();
}

bool AudioController::audioEnabled() const
{
    if (!d->resource)
        return false;

    static constexpr bool kAudioEnabledByDefault = true;

    const auto data = QJson::deserialized<SystemDataHash>(qnSettings->audioSettings());
    const auto itUserData = data.find(d->localSystemId);
    if (itUserData == data.end())
        return kAudioEnabledByDefault;

    const auto& userData = itUserData.value();
    const auto itAudioData = userData.find(d->currentUser);
    if (itAudioData == userData.end())
        return kAudioEnabledByDefault;

    const auto& audioData = itAudioData.value();
    const auto itEnabled = audioData.find(d->resource->getId());
    return itEnabled == audioData.end()
        ? kAudioEnabledByDefault
        : *itEnabled;
}

} // nx::vms::client::mobile

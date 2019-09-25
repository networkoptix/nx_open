#include "audio_controller.h"

#include <QtQml/QtQml>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions.h>
#include <mobile_client/mobile_client_settings.h>

using AudioEnabled = bool;
using AudioDataHash = QHash<QnUuid, AudioEnabled>;
using UserDataHash = QHash<QString, AudioDataHash>;
using SystemDataHash = QHash<QnUuid, UserDataHash>;

namespace nx::client::mobile {

struct AudioController::Private
{
    Private();

    QnUuid resourceId;
    SystemDataHash data;
    QnUuid localSystemId;
    QString user;

    bool enableAudioValue() const;
    void loadAudioSettings();
    void saveAudioSettings() const;
};

AudioController::Private::Private()
{
    loadAudioSettings();
}

bool AudioController::Private::enableAudioValue() const
{
    const auto itUserData = data.find(localSystemId);
    if (itUserData == data.end())
        return true;

    const auto& userData = itUserData.value();
    const auto itAudioData = userData.find(user);
    if (itAudioData == userData.end())
        return true;

    const auto& audioData = itAudioData.value();
    const auto itEnabled = audioData.find(resourceId);
    return itEnabled == audioData.end() || *itEnabled;
}

void AudioController::Private::loadAudioSettings()
{
    data = QJson::deserialized<SystemDataHash>(qnSettings->audioSettings());
}

void AudioController::Private::saveAudioSettings() const
{
    const auto settings = QJson::serialized(data);
    qnSettings->setAudioSettings(settings);
}

//--------------------------------------------------------------------------------------------------

void AudioController::registerQmlType()
{
    qmlRegisterUncreatableType<AudioController>("Nx.Mobile", 1, 0,
        "AudioController", "Cannot create an instance of AudioController.");
}

AudioController::AudioController(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &AudioController::resourceIdChanged, this, &AudioController::audioEnabledChanged);
}

AudioController::~AudioController()
{
}

void AudioController::setSessionParameters(const QnUuid& localSystemId, const QString& user)
{
    d->localSystemId = localSystemId;
    d->user = user;

    emit audioEnabledChanged();
}

void AudioController::setResourceId(const QnUuid& value)
{
    if (d->resourceId == value)
        return;

    d->resourceId = value;
    emit resourceIdChanged();
}

QnUuid AudioController::resourceId() const
{
    return d->resourceId;
}

void AudioController::setAudioEnabled(bool value)
{
    const bool enabled = d->enableAudioValue();
    if (enabled == value)
        return;


    d->data[d->localSystemId][d->user][d->resourceId] = value;
    d->saveAudioSettings();

    emit audioEnabledChanged();
}

bool AudioController::audioEnabled() const
{
    return d->enableAudioValue();
}

} // nx::client::mobile

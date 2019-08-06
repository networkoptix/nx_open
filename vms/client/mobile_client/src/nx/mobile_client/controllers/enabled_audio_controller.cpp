#include "enabled_audio_controller.h"

#include <QtQml/QtQml>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions.h>
#include <mobile_client/mobile_client_settings.h>

using EnabledAudioHash = QHash<QString, bool>;
using UserDataHash = QHash<QString, EnabledAudioHash>;
using SystemDataHash = QHash<QnUuid, UserDataHash>;

namespace nx::client::mobile {

struct EnabledAudioController::Private
{
    Private();

    QString resourceId;
    SystemDataHash data;
    QnUuid localSystemId;
    QString user;

    bool enableAudioValue();
    void loadAudioSettings();
    void saveAudioSettings();
};

EnabledAudioController::Private::Private()
{
    loadAudioSettings();
}

bool EnabledAudioController::Private::enableAudioValue()
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

void EnabledAudioController::Private::loadAudioSettings()
{
    data = QJson::deserialized<SystemDataHash>(qnSettings->enabledAudioSettings());
}

void EnabledAudioController::Private::saveAudioSettings()
{
    const auto settings = QJson::serialized(data);
    qnSettings->setEnabledAudioSettings(settings);
}

//--------------------------------------------------------------------------------------------------

void EnabledAudioController::registerQmlType()
{
    qmlRegisterUncreatableType<EnabledAudioController>("Nx.Mobile", 1, 0,
        "EnabledAudioController", "Cannot create an instance of EnabledAudioController.");
}

EnabledAudioController::EnabledAudioController(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &EnabledAudioController::resourceIdChanged,
        this, &EnabledAudioController::audioEnabledChanged);
}

EnabledAudioController::~EnabledAudioController()
{
}

void EnabledAudioController::setSessionParameters(const QnUuid& localSystemId, const QString& user)
{
    d->localSystemId = localSystemId;
    d->user = user;

    emit audioEnabledChanged();
}

void EnabledAudioController::setResourceId(const QString& value)
{
    if (d->resourceId == value)
        return;

    d->resourceId = value;
    emit resourceIdChanged();
}

QString EnabledAudioController::resourceId() const
{
    return d->resourceId;
}

void EnabledAudioController::setAudioEnabled(bool value)
{
    const bool enabled = d->enableAudioValue();
    if (enabled == value)
        return;


    d->data[d->localSystemId][d->user][d->resourceId] = value;
    d->saveAudioSettings();

    emit audioEnabledChanged();
}

bool EnabledAudioController::audioEnabled() const
{
    return d->enableAudioValue();
}

} // nx::client::mobile

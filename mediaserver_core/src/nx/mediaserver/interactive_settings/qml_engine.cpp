#include "qml_engine.h"

#include <atomic>

#include <QtCore/QPointer>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>

#include <nx/utils/log/log.h>

#include "components/items.h"

namespace nx::mediaserver::interactive_settings {

class QmlEngine::Private
{
public:
    Private();

public:
    std::unique_ptr<QQmlEngine> engine{new QQmlEngine()};
    std::unique_ptr<QQmlComponent> component{new QQmlComponent(engine.get())};
};

QmlEngine::Private::Private()
{
    components::Factory::registerTypes();

    engine->setImportPathList({});
    engine->setPluginPathList({});
}

QmlEngine::QmlEngine(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(d->component.get(), &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status)
        {
            if (status != QQmlComponent::Ready)
            {
                if (status == QQmlComponent::Error)
                {
                    setStatus(Status::error);

                    for (const auto& error: d->component->errors())
                        NX_ERROR(this, error.toString());
                }

                return;
            }

            const auto settingsItem = qobject_cast<components::Settings*>(d->component->create());
            if (!settingsItem)
            {
                setStatus(Status::error);
                NX_ERROR(this, lm("Can't load %1. Root item must be Settings.").arg(
                    d->component->url()));
                return;
            }

            setSettingsItem(settingsItem);
    });
}

QmlEngine::~QmlEngine()
{
}

void QmlEngine::load(const QByteArray& data)
{
    d->component->setData(data, QUrl());
}

void QmlEngine::load(const QString& fileName)
{
    d->component->loadUrl(fileName);
}

} // namespace nx::mediaserver::interactive_settings

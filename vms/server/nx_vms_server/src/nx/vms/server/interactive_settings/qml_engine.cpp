#include "qml_engine.h"

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>

#include "components/items.h"

namespace nx::vms::server::interactive_settings {

class QmlEngine::Private: public QObject
{
public:
    Private();

public:
    std::unique_ptr<QQmlEngine> engine{new QQmlEngine()};
    std::unique_ptr<QQmlComponent> component{new QQmlComponent(engine.get())};
    Error lastError = ErrorCode::ok;
};

QmlEngine::Private::Private()
{
    components::Factory::registerTypes();

    engine->setImportPathList({});
    engine->setPluginPathList({});
}

QmlEngine::QmlEngine():
    d(new Private())
{
    QObject::connect(d->component.get(), &QQmlComponent::statusChanged, d.get(),
        [this](QQmlComponent::Status status)
        {
            if (status != QQmlComponent::Ready)
            {
                if (status == QQmlComponent::Error)
                {
                    d->lastError = Error(ErrorCode::parseError, "QML engine reported errors:");

                    for (const auto& error: d->component->errors())
                    {
                        d->lastError.message.append(L'\n');
                        d->lastError.message.append(error.toString());
                    }
                }
                return;
            }

            const auto settingsItem = qobject_cast<components::Settings*>(d->component->create());
            if (!settingsItem)
            {
                d->lastError = Error(ErrorCode::parseError, "Root item must have Settings type.");
                return;
            }

            d->lastError = setSettingsItem(settingsItem);
        });
}

QmlEngine::~QmlEngine()
{
}

AbstractEngine::Error QmlEngine::loadModelFromData(const QByteArray& data)
{
    d->component->setData(data, QUrl());
    return d->lastError;
}

AbstractEngine::Error QmlEngine::loadModelFromFile(const QString& fileName)
{
    d->component->loadUrl(fileName);
    return d->lastError;
}

} // namespace nx::vms::server::interactive_settings

#include "qml_engine.h"

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>

#include "components/section.h"
#include "components/factory.h"

namespace nx::vms::server::interactive_settings {

class QmlEngine::Private: public QObject
{
    QmlEngine* q = nullptr;
public:
    Private(QmlEngine *q);

    void handleQmlComponentStatusChanged(QQmlComponent::Status status);

public:
    std::unique_ptr<QQmlEngine> engine{new QQmlEngine()};
    std::unique_ptr<QQmlComponent> component{new QQmlComponent(engine.get())};
};

QmlEngine::Private::Private(QmlEngine* q):
    q(q)
{
    components::Factory::registerTypes();

    engine->setImportPathList({});
    engine->setPluginPathList({});
}

void QmlEngine::Private::handleQmlComponentStatusChanged(QQmlComponent::Status status)
{
    if (status != QQmlComponent::Ready)
    {
        if (status == QQmlComponent::Error)
        {
            QString message = "QML engine reported errors:";
            for (const auto& error: component->errors())
            {
                message.append(L'\n');
                message.append(error.toString());
            }

            q->addIssue(Issue(Issue::Type::error, Issue::Code::parseError, message));

            q->stopUpdatingValues();
        }
        return;
    }

    std::unique_ptr<components::Item> rootItem(
        qobject_cast<components::Item*>(component->create()));

    q->stopUpdatingValues();

    if (!rootItem)
    {
        q->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
            "Root item is not recognized."));
        return;
    }

    if (q->setSettingsItem(std::move(rootItem)))
        q->initValues();
}

QmlEngine::QmlEngine():
    d(new Private(this))
{
    d->engine->rootContext()->setContextProperty(
        components::Item::kInterativeSettingsEngineProperty, this);

    QObject::connect(d->component.get(), &QQmlComponent::statusChanged, d.get(),
        &Private::handleQmlComponentStatusChanged);
}

QmlEngine::~QmlEngine()
{
}

bool QmlEngine::loadModelFromData(const QByteArray& data)
{
    startUpdatingValues();
    d->component->setData(data, QUrl());
    return settingsItem() != nullptr && !hasErrors();
}

bool QmlEngine::loadModelFromFile(const QString& fileName)
{
    startUpdatingValues();
    d->component->loadUrl(fileName);
    return settingsItem() != nullptr && !hasErrors();
}

} // namespace nx::vms::server::interactive_settings

#include "json_engine.h"

#include <functional>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QFile>

#include <nx/utils/log/log.h>

#include "components/items.h"

namespace nx::mediaserver::interactive_settings {

namespace {

using namespace components;

Item* createItem(Item* parent, const QJsonObject& object)
{
    const auto type = object["type"].toString();
    if (type.isEmpty())
        return nullptr;

    Item* item = Factory::createItem(type, parent);
    if (!item)
        return nullptr;

    const auto metaObject = item->metaObject();
    const int skippedProperties = QObject::staticMetaObject.propertyCount();
    for (int i = skippedProperties; i < metaObject->propertyCount(); ++i)
    {
        const auto property = metaObject->property(i);
        if (!property.isWritable())
            continue;

        const auto value = object[property.name()].toVariant();
        if (!value.isValid())
            continue;

        if (!property.write(item, value))
        {
            NX_WARNING(typeid(JsonEngine), lm("Cannot write value %1 to property %2").args(
                value, property.name()));
        }
    }

    if (const auto group = qobject_cast<Group*>(item))
    {
        auto itemsProperty = group->items();

        for (const QJsonValue& itemValue: object["items"].toArray())
        {
            const QJsonObject itemObject = itemValue.toObject();
            Item* childItem = createItem(item, itemObject);

            if (childItem)
                itemsProperty.append(&itemsProperty, childItem);
        }
    }

    return item;
}

} // namespace

JsonEngine::JsonEngine(QObject* parent):
    base_type(parent)
{
    components::Factory::registerTypes();
}

JsonEngine::~JsonEngine() = default;

void JsonEngine::load(const QJsonObject& json)
{
    auto object = json;
    object["type"] = "Settings";
    setSettingsItem(static_cast<Settings*>(createItem(nullptr, object)));
}

void JsonEngine::load(const QByteArray& data)
{
    load(QJsonDocument::fromJson(data).object());
}

void JsonEngine::load(const QUrl& url)
{
    QString path;

    if (url.isLocalFile())
        path = url.toLocalFile();
    else if (url.scheme() == "qrc")
        path = ":" + url.path();
    else if (url.scheme().isEmpty())
        path = url.path();

    if (path.isEmpty())
    {
        NX_ERROR(this, "Loading from remote URLs is not supported.");
        setStatus(Status::error);
        return;
    }

    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(this, lm("Cannot open file %1.").arg(file.fileName()));
        setStatus(Status::error);
        return;
    }

    constexpr qint64 kMaxFileSize = 1024 * 1024;
    if (file.size() > kMaxFileSize)
    {
        NX_ERROR(this, lm("Cannot load file %1. File is too big.").arg(file.fileName()));
        setStatus(Status::error);
        return;
    }

    const QByteArray data = file.readAll();

    file.close();

    load(data);

    return;
}

} // namespace nx::mediaserver::interactive_settings

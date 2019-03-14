#include "json_engine.h"

#include <functional>
#include <memory>
#include <variant>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include <nx/utils/log/log.h>

#include "components/items.h"

namespace nx::vms::server::interactive_settings {

namespace {

using namespace components;

std::variant<AbstractEngine::Error, Item*> createItem(Item* parent, const QJsonObject& object)
{
    const auto type = object["type"].toString();
    if (type.isEmpty())
    {
        return AbstractEngine::Error(
            AbstractEngine::ErrorCode::parseError,
            lm("Object does not have type: %1").arg(
                QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact))));
    }

    std::unique_ptr<Item> item(Factory::createItem(type, parent));
    if (!item)
    {
        return AbstractEngine::Error(AbstractEngine::ErrorCode::parseError,
            lm("Unknown item type: %1").arg(type));
    }

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

        if (!property.write(item.get(), value))
        {
            NX_WARNING(NX_SCOPE_TAG, lm("Cannot write value %1 to property %2").args(
                value, property.name()));
        }
    }

    if (const auto group = qobject_cast<Group*>(item.get()))
    {
        auto itemsProperty = group->items();

        for (const QJsonValue itemValue: object["items"].toArray())
        {
            const QJsonObject itemObject = itemValue.toObject();
            const auto result = createItem(item.get(), itemObject);

            if (const auto childItem = std::get_if<Item*>(&result))
                itemsProperty.append(&itemsProperty, *childItem);
            else
                return result;
        }
    }

    return item.release();
}

} // namespace

JsonEngine::JsonEngine()
{
    components::Factory::registerTypes();
}

AbstractEngine::Error JsonEngine::loadModelFromJsonObject(const QJsonObject& json)
{
    auto object = json;
    object["type"] = "Settings";

    const auto result = createItem(nullptr, object);
    if (const auto item = std::get_if<Item*>(&result))
        return setSettingsItem(static_cast<Settings*>(*item));

    return std::get<Error>(result);
}

AbstractEngine::Error JsonEngine::loadModelFromData(const QByteArray& data)
{
    QJsonParseError error;
    const auto& json = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        return Error(ErrorCode::parseError, error.errorString());

    return loadModelFromJsonObject(json.object());
}

} // namespace nx::vms::server::interactive_settings

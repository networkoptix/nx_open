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

QString processItemTemplateString(QString str, const int index, const int depth)
{
    QString result;

    enum class State
    {
        idle,
        sharpRead,
        braceRead,
        readingNumber,
    };

    State state = State::idle;
    QString number;

    const auto indexStr = QString::number(index);

    // Append non-specific character at the end to finish processing states.
    str.append(L'$');

    for (int i = 0; i < str.length(); ++i)
    {
        const auto ch = str[i];

        switch (state)
        {
            case State::idle:
                if (ch == L'#')
                    state = State::sharpRead;
                else
                    result += ch;
                break;

            case State::sharpRead:
                if (ch == L'#')
                {
                    result += ch;
                    state = State::idle;
                }
                else if (ch.isDigit())
                {
                    number += ch;
                    state = State::readingNumber;
                }
                else if (ch == L'{')
                {
                    state = State::braceRead;
                }
                else if (depth == 1)
                {
                    result += indexStr;
                    result += ch;
                    state = State::idle;
                }
                else
                {
                    result += L'#';
                    result += ch;
                    state = State::idle;
                }

                break;

            case State::braceRead:
                if (ch.isDigit())
                {
                    number += ch;
                }
                else if (ch == L'}')
                {
                    const int num = number.toInt();
                    if (num == depth)
                        result += indexStr;
                    else
                        result += "#{" + number + "}";

                    number.clear();

                    state = State::idle;
                }
                else
                {
                    result += (depth == 1)
                        ? indexStr + "{"
                        : "#{";
                    result += number;
                    number.clear();

                    // Need to reprocess the character.
                    state = State::idle;
                    --i;
                }
                break;

            case State::readingNumber:
                if (ch.isDigit())
                {
                    number += ch;
                }
                else
                {
                    const int num = number.toInt();
                    if (num == depth)
                        result += indexStr;
                    else
                        result += L'#' + number;

                    number.clear();

                    // Need to reprocess the character.
                    state = State::idle;
                    --i;
                }
                break;
        }
    }

    result.chop(1);

    return result;
}

QJsonValue processItemTemplate(const QJsonValue& value, const int index, const int depth)
{
    switch (value.type())
    {
        case QJsonValue::String:
        {
            return processItemTemplateString(value.toString(), index, depth);
        }

        case QJsonValue::Object:
        {
            QJsonObject result;

            const QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                result[it.key()] = processItemTemplate(it.value(), index, depth);

            return result;
        }

        case QJsonValue::Array:
        {
            QJsonArray array;

            for (const auto item: value.toArray())
                array.append(processItemTemplate(item, index, depth));

            return array;
        }

        default:
            break;
    }

    return value;
}

std::variant<AbstractEngine::Error, Item*> createItem(
    Item* parent, const QJsonObject& object, int repeaterDepth)
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
            const auto result = createItem(item.get(), itemObject, repeaterDepth);

            if (const auto childItem = std::get_if<Item*>(&result))
                itemsProperty.append(&itemsProperty, *childItem);
            else
                return result;
        }
    }

    if (const auto sectionContainer = qobject_cast<SectionContainer*>(item.get()))
    {
        auto sectionsProperty = sectionContainer->sections();

        for (const QJsonValue sectionValue: object["sections"].toArray())
        {
            const auto result = createItem(item.get(), sectionValue.toObject(), repeaterDepth);
            const auto childItem = std::get_if<Item*>(&result);
            if (!childItem)
                return result;

            const auto childSection = qobject_cast<Section*>(*childItem);
            if (!childSection)
            {
                return AbstractEngine::Error(AbstractEngine::ErrorCode::parseError,
                    "A section must be of type \"Section\"");
            }

            sectionsProperty.append(&sectionsProperty, childSection);
        }
    }

    if (const auto repeater = qobject_cast<Repeater*>(item.get()))
    {
        const auto& itemTemplate = QJsonObject::fromVariantMap(repeater->itemTemplate().toMap());
        const int count = repeater->count();
        const int startIndex = repeater->startIndex();
        auto itemsProperty = repeater->items();

        for (int i = 0; i < count; ++i)
        {
            const QJsonObject& model =
                processItemTemplate(itemTemplate, i + startIndex, repeaterDepth).toObject();
            const auto result = createItem(repeater, model, repeaterDepth + 1);

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

    const auto result = createItem(nullptr, object, 1);
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

#include "json_engine.h"

#include <functional>
#include <memory>
#include <variant>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>

#include "components/group.h"
#include "components/section.h"
#include "components/repeater.h"
#include "components/factory.h"

namespace nx::vms::server::interactive_settings {

namespace {

using namespace components;

static const nx::utils::log::Tag kLogTag{typeid(JsonEngine)};

static QString processItemTemplateString(QString str, const int index, const int depth)
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

static QJsonValue processItemTemplate(const QJsonValue& value, const int index, const int depth)
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

static std::unique_ptr<Item> createItem(
    JsonEngine* engine, Item* parent, const QJsonObject& itemObject, int repeaterDepth)
{
    QString type = itemObject["type"].toString();
    if (type.isEmpty())
    {
        if (!parent)
        {
            // For the top-level Item of a Settings Model, the field "type" is optional.
            type = "Settings";
        }
        else
        {
            engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
                lm("Item field \"type\" is missing or empty in the Item defined by JSON %1").arg(
                    QString::fromUtf8(QJsonDocument(itemObject).toJson(QJsonDocument::Compact)))));
            return {};
        }
    }

    std::unique_ptr<Item> item(Factory::createItem(type, parent));
    if (!item)
    {
        engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
            lm("Item field \"type\" has unknown value: %1").arg(nx::kit::utils::toString(type))));
        return {};
    }

    item->setEngine(engine);

    const auto group = qobject_cast<Group*>(item.get());
    const auto sectionContainer = qobject_cast<SectionContainer*>(item.get());
    const auto repeater = qobject_cast<Repeater*>(item.get());

    const auto& writeProperty =
        [engine, type, item = item.get(), metaObject = item->metaObject()](
            const QString& key, const QJsonValue& value)
        {
            const int propertyIndex = metaObject->indexOfProperty(key.toUtf8().constData());
            if (propertyIndex == -1)
            {
                if (key.startsWith(L'_'))
                {
                    // This allows us to leave comments or metadata in the JSON.
                    return;
                }

                engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
                    lm("Property \"%1\" does not exist in \"%2\".").args(key, type)));
                return;
            }

            const auto property = metaObject->property(propertyIndex);
            if (!property.isWritable())
            {
                engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
                    lm("Property \"%1\" of \"%2\" is read-only.").args(key, type)));
                return;
            }

            if (!property.write(item, value.toVariant()))
                NX_WARNING(kLogTag, "Cannot write value %1 to property \"%2\".", value, key);
        };

    for (auto it = itemObject.begin(); it != itemObject.end(); ++it)
    {
        const QString& key = it.key();

        if (key.startsWith("_"))
            continue; //< Ignore JSON comments and private fields.

        if (key == QStringLiteral("value"))
        {
            // In VMS v4.0 Analytics Plugin, the "value" field was sometimes present in the
            // Settings Models in Manifests, thus, for compatibility with such plugins, no Issue
            // is raised here, just a warning is logged.
            NX_WARNING(kLogTag, "Settings Model contains a \"value\" field; ignored.");
            continue;
        }

        // Skip the specific keys.
        if (key == QStringLiteral("type"))
            continue;
        if (group && key == QStringLiteral("items"))
            continue;
        if (sectionContainer && key == QStringLiteral("sections"))
            continue;
        if (repeater && key == QStringLiteral("items"))
        {
            engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
                "Repeater cannot have \"items\"; use \"template\" instead."));
            return {};
        }

        writeProperty(key, it.value());
    }

    if (group)
    {
        auto itemsProperty = group->items();

        for (const QJsonValue itemValue: itemObject["items"].toArray())
        {
            const QJsonObject itemObject = itemValue.toObject();
            auto childItem = createItem(engine, item.get(), itemObject, repeaterDepth);

            if (childItem)
                itemsProperty.append(&itemsProperty, childItem.release());
            else
                return {};
        }
    }

    if (sectionContainer)
    {
        auto sectionsProperty = sectionContainer->sections();

        for (const QJsonValue sectionValue: itemObject["sections"].toArray())
        {
            auto childItem = createItem(
                engine, item.get(), sectionValue.toObject(), repeaterDepth);
            if (!childItem)
                return {};

            const auto childSection = qobject_cast<Section*>(childItem.get());
            if (!childSection)
            {
                engine->addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
                    lm("\"sections\" must contain items of \"Section\" type, but \"%1\" is found.")
                        .arg(childItem->type())));
                return {};
            }

            childItem.release();
            sectionsProperty.append(&sectionsProperty, childSection);
        }
    }

    if (repeater)
    {
        const auto& itemTemplate = repeater->itemTemplate();
        const int count = repeater->count();
        const int startIndex = repeater->startIndex();
        auto itemsProperty = repeater->items();

        for (int i = 0; i < count; ++i)
        {
            const QJsonObject& model =
                processItemTemplate(itemTemplate, i + startIndex, repeaterDepth).toObject();
            auto childItem = createItem(engine, repeater, model, repeaterDepth + 1);

            if (childItem)
                itemsProperty.append(&itemsProperty, childItem.release());
            else
                return {};
        }
    }

    return item;
}

} // namespace

JsonEngine::JsonEngine()
{
    components::Factory::registerTypes();
}

bool JsonEngine::loadModelFromJsonObject(const QJsonObject& json)
{
    startUpdatingValues();
    auto item = createItem(this, /*parent*/ nullptr, json, 1);
    stopUpdatingValues();

    if (!item)
        return false;

    if (!setSettingsItem(std::move(item)))
        return false;

    initValues();

    return !hasErrors();
}

bool JsonEngine::loadModelFromData(const QByteArray& data)
{
    QJsonParseError error;
    const auto& json = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
    {
        addIssue(Issue(Issue::Type::error, Issue::Code::parseError, error.errorString()));
        return false;
    }

    return loadModelFromJsonObject(json.object());
}

} // namespace nx::vms::server::interactive_settings

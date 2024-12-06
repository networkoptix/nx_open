// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

namespace nx::vms::client::core::testkit::utils {

namespace {

const QString kRegexPrefix = "re:";

QString stripShortcuts(QString title)
{
    // Replace: &A -> A
    static const QRegularExpression amp("&(.)");
    return title.replace(amp, "\\1");
}

bool objectHasId(const QObject* object, const QString& id)
{
    auto c = qmlContext(object);
    return c && c->nameForObject(const_cast<QObject*>(object)) == id;
}

bool valueIsTrue(const QJSValue& value)
{
    return value.toString() == "yes" || value.toInt() == 1 || value.toBool();
}

bool hasRegexPreffix(const QString& str)
{
    return str.contains(kRegexPrefix);
}

QString extractRegex(const QString& str)
{
    return str.mid(kRegexPrefix.size());
}

} // namespace

bool textMatches(QString itemText, QString text)
{
    if (hasRegexPreffix(text))
        return itemText.contains(QRegularExpression(extractRegex(text)));

    return itemText == text || stripShortcuts(itemText) == text;
}

bool objectMatches(const QObject* object, QJSValue properties)
{
    if (!object)
        return false;

    if (properties.isString())
    {
        const auto s = properties.toString();

        const auto text = object->property("text");
        if (!text.isNull() && textMatches(text.toString(), s))
            return true;

        const auto title = object->property("title");
        return !title.isNull() && textMatches(title.toString(), s);
    }

    if (properties.hasOwnProperty("name"))
    {
        if (object->objectName() != properties.property("name").toString())
            return false;
    }

    if (properties.hasOwnProperty("objectName"))
    {
        if (object->objectName() != properties.property("objectName").toString())
            return false;
    }

    if (properties.hasOwnProperty("id"))
    {
        if (!objectHasId(object, properties.property("id").toString()))
            return false;
    }

    if (properties.hasOwnProperty("unnamed"))
    {
        if (!object->objectName().isEmpty())
            return false;
    }

    if (properties.hasOwnProperty("toolTip"))
    {
        const auto toolTip = object->property("toolTip").toString();
        if (!toolTip.isEmpty())
        {
            if (!textMatches(toolTip, properties.property("toolTip").toString()))
                return false;
        }
    }

    if (properties.hasOwnProperty("labelText"))
    {
        const auto labelText = object->property("labelText").toString();
        if (!labelText.isEmpty())
        {
            if (!textMatches(labelText, properties.property("labelText").toString()))
                return false;
        }
    }

    if (properties.hasOwnProperty("type"))
    {
        const auto className = QString(object->metaObject()->className());
        const auto typeString = properties.property("type").toString();
        bool matches = false;

        // QML type name looks like QQuickText or Text_QMLTYPE_123
        if (qobject_cast<const QQuickItem*>(object) || qobject_cast<const QQuickWindow*>(object))
        {
            // TODO: Also use QML.Element from classInfo.
            matches = className.startsWith(typeString + "_QMLTYPE_") ||
                className.startsWith("QQuick" + typeString) ||
                className.startsWith("QQuickPre64" + typeString);
        }

        if (!matches)
        {
            if (auto metaType = QMetaType::fromName(typeString.toUtf8()); metaType.isValid())
                matches = QMetaType::canView(object->metaObject()->metaType(), metaType);
        }

        if (!matches && className != typeString)
            return false;
    }

    static const QList<const char*> strProperties{
        "source",
        "text",
        "title",
    };

    for (const auto i: strProperties)
    {
        if (!properties.hasOwnProperty(i))
            continue;

        if (!textMatches(object->property(i).toString(), properties.property(i).toString()))
            return false;
    }

    static const QList<const char*> intProperties{
        "row",
        "column",
        "x",
        "y",
        "z",
    };

    for (const auto i: intProperties)
    {
        if (!properties.hasOwnProperty(i))
            continue;

        if (object->property(i).toInt() != properties.property(i).toInt())
            return false;
    }

    static const QList<const char*> boolProperties{
        "enabled",
        "visible",
        "selected",
    };

    for (const auto i: boolProperties)
    {
        if (!properties.hasOwnProperty(i))
            continue;

        if (object->property(i).toBool() != valueIsTrue(properties.property(i)))
            return false;
    }

    // TODO: handle "leftWidget" and "aboveWidget" properties.

    return true;
}

} // namespace nx::vms::client::core::testkit::utils

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QWidget>

#include "graphics_item_wrapper.h"
#include "model_index_wrapper.h"
#include "tab_item_wrapper.h"

namespace nx::vms::client::desktop::testkit::utils {

namespace {

const QString kRegexPrefix = "re:";

QString stripShortcuts(QString title)
{
    // Replace: &A -> A
    static const QRegularExpression amp("&(.)");
    return title.replace(amp, "\\1");
}

bool sidesWithAny(QVariant object, QVariantList sideWidgets, Qt::Alignment side)
{
    if (sideWidgets.isEmpty())
        return false;

    const auto objectRect = globalRect(object);
    return std::any_of(sideWidgets.cbegin(), sideWidgets.cend(),
        [objectRect, side](QVariant sideWidget)
        {
            return sideIntersect(objectRect, globalRect(sideWidget), side) > 0;
        });
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

        if (const auto w = dynamic_cast<const QGraphicsItem*>(object))
        {
            if (!textMatches(w->toolTip(), properties.property("toolTip").toString()))
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
    if (properties.hasOwnProperty("window"))
    {
        const auto w = qobject_cast<const QWidget*>(object);
        if (!objectMatches(w->window(), properties.property("window")))
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

    static const QMap<const char *, Qt::Alignment> sideMap{
        { "leftWidget", Qt::AlignLeft },
        { "aboveWidget", Qt::AlignTop },
    };

    // Check all side widgets (if present)
    for (auto i = sideMap.constBegin(); i != sideMap.constEnd(); ++i)
    {
        if (!properties.hasOwnProperty(i.key()))
            continue;

        const auto sideWidgets = findAllObjects(properties.property(i.key()));
        const auto side = i.value();

        // If none of the side widgets are on correct side - then current object does not match.
        if (!sidesWithAny(QVariant::fromValue(const_cast<QObject*>(object)), sideWidgets, side))
            return false;
    }

    return true;
}

bool indexMatches(QModelIndex index, QJSValue properties)
{
    if (!index.isValid())
        return false;

    if (properties.isString())
        return textMatches(index.data().toString(), properties.toString());

    // We are looking for an object, not QModelIndex.
    if (properties.hasOwnProperty("name") || properties.hasOwnProperty("id"))
        return false;

    if (properties.hasOwnProperty("text"))
    {
        if (!textMatches(index.data().toString(), properties.property("text").toString()))
            return false;
    }

    if (properties.hasOwnProperty("row"))
    {
        if (index.row() != properties.property("row").toInt())
            return false;
    }

    if (properties.hasOwnProperty("column"))
    {
        if (index.column() != properties.property("column").toInt())
            return false;
    }

    if (properties.hasOwnProperty("checkState"))
    {
        const auto p = properties.property("checkState");
        const auto state = index.data(Qt::CheckStateRole);
        if (p.isNumber())
        {
            if (state.toInt() != p.toInt())
                return false;
        }
        else
        {
            if (ModelIndexWrapper::checkStateName(state) != p.toString())
                return false;
        }
    }

    if (properties.hasOwnProperty("type"))
    {
        if (properties.property("type").toString() != "QModelIndex")
            return false;
    }

    return true;
}

bool tabItemMatches(const QTabBar* tabBar, int index, QJSValue properties)
{
    if (index < 0 || index >= tabBar->count())
        return false;

    if (properties.isString())
        return textMatches(tabBar->tabText(index), properties.toString());

    if (properties.hasOwnProperty("name"))
        return false;

    if (properties.hasOwnProperty("type"))
    {
        if (properties.property("type").toString() != TabItemWrapper::type())
            return false;
    }

    if (properties.hasOwnProperty("visible"))
    {
        if (valueIsTrue(properties.property("visible")) != tabBar->isTabVisible(index))
            return false;
    }
    if (properties.hasOwnProperty("enabled"))
    {
        if (valueIsTrue(properties.property("enabled")) != tabBar->isTabEnabled(index))
            return false;
    }
    if (properties.hasOwnProperty("text"))
    {
        if (!textMatches(tabBar->tabText(index), properties.property("text").toString()))
            return false;
    }
    if (properties.hasOwnProperty("toolTip"))
    {
        if (!textMatches(tabBar->tabToolTip(index), properties.property("toolTip").toString()))
            return false;
    }
    return true;
}

bool graphicsItemMatches(QGraphicsItem* item, QJSValue properties)
{
    const GraphicsItemWrapper wrapper(item);

    if (properties.hasOwnProperty("type"))
    {
        const auto typeName = properties.property("type").toString();
        if (typeName != wrapper.type() && typeName != "QGraphicsItem")
            return false;
    }

    if (properties.hasOwnProperty("visible"))
    {
        if (valueIsTrue(properties.property("visible")) != item->isVisible())
            return false;
    }
    if (properties.hasOwnProperty("enabled"))
    {
        if (valueIsTrue(properties.property("enabled")) != item->isEnabled())
            return false;
    }

    if (properties.isString())
        return textMatches(wrapper.text(), properties.toString());

    if (properties.hasOwnProperty("text"))
    {
        if (!textMatches(wrapper.text(), properties.property("text").toString()))
            return false;
    }
    if (properties.hasOwnProperty("toolTip"))
    {
        if (!textMatches(item->toolTip(), properties.property("toolTip").toString()))
            return false;
    }

    return true;
}

} // namespace nx::vms::client::desktop::testkit::utils

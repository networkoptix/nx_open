// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/testkit/utils.h>

#include "graphics_item_wrapper.h"
#include "model_index_wrapper.h"
#include "tab_item_wrapper.h"

namespace nx::vms::client::desktop::testkit::utils {

namespace {

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

bool valueIsTrue(const QJSValue& value)
{
    return value.toString() == "yes" || value.toInt() == 1 || value.toBool();
}

bool textMatches(QString itemText, QString text)
{
    return core::testkit::utils::textMatches(itemText, text);
}

} // namespace

bool objectMatches(const QObject* object, QJSValue properties)
{
    if (!core::testkit::utils::objectMatches(object, properties))
        return false;

    // Important! Add only QWidget-related checks here.
    // Everythin else goes to core::testkit::utils::objectMatches

    if (properties.hasOwnProperty("toolTip"))
    {
        if (const auto w = dynamic_cast<const QGraphicsItem*>(object))
        {
            if (!textMatches(w->toolTip(), properties.property("toolTip").toString()))
                return false;
        }
    }

    if (properties.hasOwnProperty("window"))
    {
        const auto w = qobject_cast<const QWidget*>(object);
        if (!w || !objectMatches(w->window(), properties.property("window")))
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

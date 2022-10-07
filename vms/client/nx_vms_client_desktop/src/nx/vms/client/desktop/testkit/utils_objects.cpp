// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QComboBox>
#include <QtCore/QSet>
#include <QtQml/QJSValueIterator>

#include "model_index_wrapper.h"
#include "tab_item_wrapper.h"


namespace nx::vms::client::desktop::testkit::utils {

namespace {

/** Return all matching indexes of the model, starting at (but not including) parent. */
QVariantList findAllIndexes(
    const QAbstractItemModel* model,
    QModelIndex parent,
    QWidget* widget,
    QJSValue properties)
{
    QVariantList result;

    const auto selectMatchingIndexes =
        [&properties, &result, widget](QModelIndex index) -> bool
        {
            if (indexMatches(index, properties))
            {
                auto v = QVariant::fromValue(ModelIndexWrapper(index, widget));
                result.append(v);
            }
            return true;
        };

    visitModel(model, parent, selectMatchingIndexes);

    return result;
}

/** Get text property representation, only if it's the only property. */
QString getTextProperty(QJSValue parameters)
{
    if (parameters.isString())
        return parameters.toString();

    QString text;
    QJSValueIterator it(parameters);
    while (it.hasNext())
    {
        it.next();
        QString name = it.name();
        if (name == "container") //< "container" property is allowed, just skip it.
            continue;
        else if (name == "text" || (name == "title" && text.isEmpty()))
            text = it.value().toString();
        else
            return QString(); //< found non-text property, abort
    }
    return text;
}

/**
 * Get matching tabs of QTab* widget.
 */
QVariantList findAllTabItems(QObject* object, QJSValue parameters)
{
    QVariantList result;

    const auto tabWidget = qobject_cast<QTabWidget*>(object);

    if (const auto w = tabWidget ? tabWidget->tabBar() : qobject_cast<QTabBar*>(object))
    {
        for (int i = 0; i < w->count(); ++i)
        {
            if (tabItemMatches(w, i, parameters))
                result << QVariant::fromValue(TabItemWrapper(i, w));
        }
    }

    return result;
}

} // namespace

QVariantList findAllObjectsInContainer(
    QVariant container,
    QJSValue properties,
    QSet<QObject*>& visited)
{
    // Check if we are trying to find children of QModelIndex.
    if (const auto wrap = container.value<ModelIndexWrapper>(); wrap.isValid())
        return findAllIndexes(wrap.index().model(), wrap.index(), wrap.container(), properties);

    // Setup visiting children of QObject.
    QVariantList result;

    auto containerObject = qvariant_cast<QObject*>(container);

    // Cache some values that would be checked when visiting children objects.
    const auto itemType = properties.property("type");
    const bool unnamed = !properties.hasOwnProperty("name");
    const bool canBeIndex = (itemType.toString() == "QModelIndex" || itemType.isUndefined())
        && unnamed;
    const QString textProperty = getTextProperty(properties);

    const bool canBeTabItem =
        (itemType.toString() == TabItemWrapper::type() || itemType.isUndefined()) && unnamed;

    const auto selectMatchingObjects =
        [&properties, &result, &visited, containerObject, canBeIndex, textProperty, canBeTabItem]
        (QObject* object) -> bool
        {
            // Remember visiting this object.
            if (visited.contains(object))
                return false;
            visited.insert(object);

            // Add matching object, but be sure that we don't include the container.
            if (containerObject != object && objectMatches(object, properties))
                result << QVariant::fromValue(object);

            // Try to match model children.
            if (canBeIndex)
            {
                const auto comboBox = qobject_cast<const QComboBox*>(object);
                if (const auto view = comboBox
                    ? comboBox->view()
                    : qobject_cast<QAbstractItemView*>(object))
                {
                    const auto model = view->model();
                    // Do not visit same model more than once.
                    if (!visited.contains(model))
                    {
                        visited.insert(model);
                        result << findAllIndexes(model, view->rootIndex(), view, properties);
                    }
                }
            }

            // Tabs in QTabBar are not separate objects so handle them in a special function.
            if (canBeTabItem && object->isWidgetType())
                result << findAllTabItems(object, properties);

            return true;
        };

    // Check if we are trying to find children of a tab in QTabBar.
    if (const auto wrap = container.value<TabItemWrapper>(); wrap.isValid())
    {
        visitTree(wrap.leftButton(), selectMatchingObjects);
        visitTree(wrap.rightButton(), selectMatchingObjects);
    }
    else
    {
        visitTree(containerObject, selectMatchingObjects);
    }

    return result;
}

QVariantList findAllObjects(QJSValue properties)
{
    // Gather containers where to search for matches
    QVariantList containers;

    // Return QObjects directly.
    if (auto qobject = properties.toQObject())
    {
        containers << QVariant::fromValue(qobject);
        return containers;
    }

    // Return special wrappers directly.
    if (const auto variantWrap = properties.toVariant(); variantWrap.isValid())
    {
        if (const auto wrap = variantWrap.value<TabItemWrapper>(); wrap.isValid())
        {
            containers << variantWrap;
            return containers;
        }
    }

    const bool hasContainer = properties.hasOwnProperty("container");

    if (hasContainer)
    {
        containers = findAllObjects(properties.property("container"));
    }
    else
    {
        // If there is no "container" property then start the search
        // from top level widgets.
        auto widgets = qApp->topLevelWidgets();
        for (QWidget* w: widgets)
            containers << QVariant::fromValue(w);
    }

    // Search only in containers.
    QVariantList matchingObjects;
    QSet<QObject*> visited; //< Avoid visiting the same QObject more than once.

    for (QVariant c: containers)
    {
        // If there was no "container" property then we should also
        // check whether container (top level widget) matches.
        if (!hasContainer && objectMatches(qvariant_cast<QObject*>(c), properties))
            matchingObjects.append(c);

        matchingObjects << findAllObjectsInContainer(c, properties, visited);
    }

    return matchingObjects;
}

} // namespace nx::vms::client::desktop::testkit::utils

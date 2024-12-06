// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QGuiApplication>
#include <QtQml/QJSValueIterator>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlContext>

#include "utils.h"

namespace nx::vms::client::core::testkit::utils {

enum VisitOption {
    NoOptions = 0x0,
    OnlyQuickItems = 0x1,
};

using VisitOptions = QFlags<VisitOption>;

void visitTree(
    QObject* object,
    std::function<bool(QObject*)> onVisited,
    VisitOptions flags = NoOptions)
{
    if (!object)
        return;

    if (flags.testFlag(OnlyQuickItems) && !qobject_cast<const QQuickItem*>(object))
        return;

    if (!onVisited(object))
        return;

    // Visual children of QQuickItem
    if (const auto root = qobject_cast<const QQuickItem*>(object))
    {
        QList<QQuickItem*> items = root->childItems();
        for (int i = 0; i < items.size(); ++i)
            visitTree(items.at(i), onVisited, flags | OnlyQuickItems);
    }

    if (const auto w = qobject_cast<const QQuickWindow*>(object))
        visitTree(w->contentItem(), onVisited, flags | OnlyQuickItems);

    const QObjectList children = object->children();
    for (int i = 0; i < children.size(); ++i)
        visitTree(children.at(i), onVisited, flags);
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

QVariantList findAllObjectsInContainer(
    QVariant container,
    QJSValue properties,
    QSet<QObject*>& visited)
{
    // Setup visiting children of QObject.
    QVariantList result;

    auto containerObject = qvariant_cast<QObject*>(container);

    // Cache some values that would be checked when visiting children objects.
    const auto itemType = properties.property("type");
    const bool unnamed = !properties.hasOwnProperty("name");

    const QString textProperty = getTextProperty(properties);

    const auto selectMatchingObjects =
        [&properties, &result, &visited, containerObject, textProperty]
        (QObject* object) -> bool
        {
            // Remember visiting this object.
            if (visited.contains(object))
                return false;
            visited.insert(object);

            // Add matching object, but be sure that we don't include the container.
            if (containerObject != object && objectMatches(object, properties))
                result << QVariant::fromValue(object);

            return true;
        };

    visitTree(containerObject, selectMatchingObjects);

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

    const bool hasContainer = properties.hasOwnProperty("container");

    if (hasContainer)
    {
        containers = findAllObjects(properties.property("container"));
    }
    else
    {
        // If there is no "container" property then start the search
        // from top level widgets.

        auto windows = qApp->topLevelWindows();
        for (QWindow* window: windows)
        {
            if (auto quickWindow = qobject_cast<QQuickWindow*>(window))
                containers << QVariant::fromValue(quickWindow);
        }
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

std::pair<QString, QString> nameAndBaseUrl(const QObject* object)
{
    QString id;
    QUrl contextLocation;

    if (auto c = qmlContext(object))
    {
        id = c->nameForObject(const_cast<QObject*>(object));

        if (!id.isEmpty() || !c->baseUrl().isEmpty())
        {
            contextLocation = c->baseUrl();
            return {id, contextLocation.toString()};
        }
    }

    return {object->objectName(), ""};
}

/** Returns QObject properties as QVarintMap. */
QVariant dumpQObject(const QObject* object, bool withChildren)
{
    if (!object)
        return QVariant::fromValue(nullptr);

    QVariantMap result;

    // Dump properties.
    for (auto mo = object->metaObject(); mo; mo = mo->superClass())
    {
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i)
            result.insert(mo->property(i).name(), mo->property(i).read(object));
    }

    auto [id, location] = nameAndBaseUrl(object);
    if (!location.isEmpty() && !id.isEmpty())
        result.insert("id", id);

    result.insert("name", object->objectName());
    result.insert("objectName", object->objectName());
    result.insert("type", object->metaObject()->className());

    if (!withChildren)
        return result;

    // Step inside Qt Quick root object.
    if (const auto w = qobject_cast<const QQuickWindow*>(object))
        object = w->contentItem();

    const QObjectList children = object->children();
    if (children.empty())
        return result;

    QVariantList resultChildren;

    for (int i = 0; i < children.size(); ++i)
        resultChildren.append(dumpQObject(children.at(i), withChildren));

    result.insert("children", resultChildren);

    return result;
}

} // namespace nx::vms::client::core::testkit::utils

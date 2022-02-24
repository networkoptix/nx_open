// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QMetaProperty>
#include <QtWidgets/QAction>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QWidget>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>


namespace nx::vms::client::desktop::testkit::utils {

std::pair<QString, QString> nameAndBaseUrl(const QObject* object)
{
    QString id;
    QUrl contextLocation;

    // Find topmost context with id or at least topmost context base URL.
    for (auto c = qmlContext(object); c; c = c->parentContext())
    {
        if (const auto name = c->nameForObject(const_cast<QObject*>(object)); !name.isEmpty())
        {
            id = name;
            contextLocation = c->baseUrl();
        }
        else if (id.isEmpty())
        {
            if (const auto baseUrl = c->baseUrl(); !baseUrl.isEmpty())
                contextLocation = baseUrl;
        }
    }

    if (!id.isEmpty() || !contextLocation.isEmpty())
        return {id, contextLocation.toString()};


    return {object->objectName(), ""};
}

QVariant dumpQModelIndex(const QAbstractItemModel* model, QModelIndex parent, bool withChildren)
{
    QVariantMap i;
    i.insert("type", "QModelIndex");

    if (parent.isValid())
    {
        i.insert("row", parent.row());
        i.insert("column", parent.column());
        i.insert("text", model->data(parent));
    }
    if (!withChildren)
        return i;

    QVariantList children;

    QVariantList modelData;
    for(int r = 0; r < model->rowCount(parent); ++r)
    {
        for(int c = 0; c < model->columnCount(parent); ++c)
        {
            QModelIndex index = model->index(r, c, parent);
            children.append(dumpQModelIndex(model, index, withChildren));
        }
    }

    i.insert("children", children);
    return i;
}

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

    if (object->isWidgetType())
    {
        const auto w = qobject_cast<const QWidget*>(object);
        if (w->isVisible())
        {
            QVariantMap geometry;
            geometry.insert("x", w->x());
            geometry.insert("y", w->y());
            geometry.insert("width", w->width());
            geometry.insert("height", w->height());
            result.insert("geometry", geometry);
        }

        QVariantList actions;

        foreach(QAction* a, w->actions())
            actions.append(dumpQObject(a, false));

        result.insert("actions", actions);

        if (const auto iv = qobject_cast<const QAbstractItemView*>(w))
        {
            auto model = iv->model();
            QModelIndex parent = iv->rootIndex();
            if (model && model->hasChildren(parent))
                result.insert("model", dumpQModelIndex(iv->model(), iv->rootIndex(), withChildren));
        }
    }

    if (!withChildren)
        return result;

    // Step inside Qt Quick root object.
    if (const auto w = qobject_cast<const QQuickWidget*>(object))
        object = w->rootObject();

    const QObjectList children = object->children();
    if (children.empty())
        return result;

    QVariantList resultChildren;

    for (int i = 0; i < children.size(); ++i)
        resultChildren.append(dumpQObject(children.at(i), withChildren));

    result.insert("children", resultChildren);

    return result;
}

} // namespace nx::vms::client::desktop::testkit::utils

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_globals.h"

#include <QtGui/QKeySequence>
#include <QtQml/QtQml>

#include <nx/build_info.h>

namespace {

QString keyToString(Qt::Key key)
{
    switch (key)
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            return "Enter";

        case Qt::Key_Control:
            return nx::build_info::isMacOsX()
                ? QKeySequence(key).toString(QKeySequence::NativeText)
                : "Ctrl";
        default:
            return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

QStringList toStringList(const QVector<Qt::Key>& keys)
{
    QStringList result;
    result.reserve(keys.size());

    for (const auto key: keys)
        result.push_back(keyToString(key));

    return result;
}

} // namespace

namespace nx::vms::client::desktop {
namespace ResourceTree {

bool isSeparatorNode(NodeType nodeType)
{
    return nodeType == NodeType::separator;
}

ShortcutHint::ShortcutHint(const QStringList& keys, const QString& description):
    keys(keys),
    description(description)
{
}

ShortcutHint::ShortcutHint(const QVector<Qt::Key>& keys, const QString& description):
    ShortcutHint(toStringList(keys), description)
{
}

bool ShortcutHint::operator==(const ShortcutHint& other) const
{
    return keys == other.keys && description == other.description;
}

bool ShortcutHint::operator!=(const ShortcutHint& other) const
{
    return !(*this == other);
}

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "ResourceTree", "ResourceTree is a namespace");

    qRegisterMetaType<NodeType>();
    qRegisterMetaType<ItemState>();
    qRegisterMetaType<ResourceExtraStatusFlag>();
    qRegisterMetaType<FilterType>();
    qRegisterMetaType<ShortcutHint>();
    qRegisterMetaType<ActivationType>();
}

} // namespace ResourceTree
} // namespace nx::vms::client::desktop

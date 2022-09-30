// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QHash>
#include <QtCore/QHashFunctions>
#include <QtCore/QStringList>
#include <QtCore/QVector>

#include <nx/reflect/enum_instrument.h>

class QKeySequence;

namespace nx::vms::client::desktop {
namespace ResourceTree {

Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

/**
 * Type of a node in resource tree. Ordered as it should be displayed.
 */
NX_REFLECTION_ENUM_CLASS(NodeType,

    // Single-occurrence nodes.

    root, //<Root node for the tree (current system node).
    currentSystem, //< Root node, displaying current system name.
    currentUser, //< Root node, displaying current user.
    separator, //< Root node for spacing between header and main part of the tree.
    servers, //< Root node for servers.
    camerasAndDevices, //< Root node for devices when representation without servers is turned on.
    layouts, //< Root node for current user's layouts and shared layouts.
    layoutTours, //< Root node for the layout tours.
    videoWalls, //< Never visible root node which represents videowalls group.
    webPages, //< Root node for web pages.
    users, //< Root node for user resources.
    analyticsEngines, //< Root node for analytics engines.
    otherSystems, //< Root node for remote systems.
    localResources, //< Root node for local resources.

    // Per-user placeholder nodes.

    roleUsers, //< 'Users' node, displayed under roles.
    allCamerasAccess, //< 'All Cameras' placeholder, displayed for users and roles with full access.
    allLayoutsAccess, //< 'All Shared Layouts' placeholder, displayed under admins.
    sharedResources, //< 'Cameras & Resources' node, displayed for custom users and roles.
    sharedLayouts, //< 'Layouts' node, displayed under users and roles with custom access.

    // Repeating nodes.

    role, //< Custom role.
    sharedLayout, //< Shared layout link, displayed under user.
    recorder, //< Recorder (VMAX, etc). Has both guid and resource (parent server).
    customResourceGroup, //< User defined group of camera resources for representation.
    sharedResource, //< Accessible resource link, displayed under user.
    resource, //< Generic resource, most of physical device. Has only resource.
    layoutItem, //< Layout item. Has both guid and resource.
    edge, //< EDGE server with a camera in a collapsed state, displayed as single camera.
    layoutTour, //< Layout tour (Showreel). Has a guid.
    videoWallItem, //< Videowall item (screen). Has a guid and can have resource.
    videoWallMatrix, //< Videowall saved matrix. Has a guid.

    cloudSystem, //< Available cloud system.
    cloudSystemStatus, //< Cross-System Resources status preloader for the Cloud System.
    localSystem, //< Local system but the current.

    // Resource dialogs specific types, never occur in the main resource tree.

    spacer //< Half-separator height dummy item used for proper alignment of pinned items.
)
Q_ENUM_NS(NodeType);

inline uint qHash(NodeType value, uint seed = 0)
{
    return ::qHash(static_cast<int>(value), seed);
}

enum class ItemState
{
    normal,
    selected,
    accented
};
Q_ENUM_NS(ItemState);

enum class CameraExtraStatusFlag
{
    empty = 0,
    recording = 1 << 0,
    scheduled = 1 << 1,
    buggy = 1 << 2,
    hasArchive = 1 << 3
};
Q_ENUM_NS(CameraExtraStatusFlag);

enum class FilterType
{
    noFilter,
    everything,
    servers,
    cameras,
    layouts,
    showreels,
    videowalls,
    webPages,
    users,
    localFiles
};
Q_ENUM_NS(FilterType);

enum class ActivationType
{
    enterKey,
    singleClick,
    doubleClick,
    middleClick
};
Q_ENUM_NS(ActivationType);

bool isSeparatorNode(NodeType nodeType);

struct ShortcutHint
{
    QStringList keys;
    QString description;

    ShortcutHint() = default;
    ShortcutHint(const QStringList& keys, const QString& description);
    ShortcutHint(const QVector<Qt::Key>& keys, const QString& description);

    bool operator==(const ShortcutHint& other) const;
    bool operator!=(const ShortcutHint& other) const;

    Q_GADGET
    Q_PROPERTY(QStringList keys MEMBER keys)
    Q_PROPERTY(QString description MEMBER description)
};

struct ExpandedNodeId
{
    NodeType type{};
    QString id;
    QString parentId;

    operator bool() const { return type != NodeType{}; }
};

inline uint qHash(const ExpandedNodeId& value, uint seed = 0)
{
    const auto combine = QtPrivate::QHashCombine();
    return combine(combine(combine(seed,
        qHash(value.type)),
        qHash(value.id)),
        qHash(value.parentId));
}

void registerQmlType();

} // namespace ResourceTree
} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ResourceTree::ShortcutHint)

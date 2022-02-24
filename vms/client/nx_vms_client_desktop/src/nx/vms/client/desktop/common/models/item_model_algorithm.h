// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>

class QAbstractItemModel;

namespace nx::vms::client::desktop {
namespace item_model {

// TODO: #vbreus Bring here all item model related, implementation independent helpers scattered
// here and there across sources.

/**
 * Return all indexes referred to items with at least one child within subtree with root item with
 * <tt>parent</tt> index. Root item itself is not included.
 */
NX_VMS_CLIENT_DESKTOP_API QModelIndexList getNonLeafIndexes(
    const QAbstractItemModel* model,
    const QModelIndex& parent = QModelIndex());

/**
 * Return all indexes referred to childless items within subtree with root item with
 * <tt>parent</tt> index. Root item itself is not included.
 */
NX_VMS_CLIENT_DESKTOP_API QModelIndexList getLeafIndexes(
    const QAbstractItemModel* model,
    const QModelIndex& parent = QModelIndex());

/**
 * Return all indexes within subtree with root item with <tt>parent</tt> index as a flat list.
 * Root item itself is not included.
 */
NX_VMS_CLIENT_DESKTOP_API QModelIndexList getAllIndexes(
    const QAbstractItemModel* model,
    const QModelIndex& parent = QModelIndex());

} // namespace item_model
} // namespace nx::vms::client::desktop

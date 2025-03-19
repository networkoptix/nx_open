// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>

namespace nx::vms::client::desktop {

class WindowContext;

/**
 * Functor which encapsulates actions performed on committing user input in a tree view
 * with Resource Tree model as source model.
 */
using TreeEditDelegate = std::function<bool(const QModelIndex&, const QVariant&)>;
TreeEditDelegate createResourceTreeEditDelegate(WindowContext* context);

} // namespace nx::vms::client::desktop

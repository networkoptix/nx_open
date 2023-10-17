// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QModelIndex;
class QVariant;

#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Functional object which encapsulates actions performed on committing user input in a tree view
 * with Resource Tree model as source model.
 */
class ResourceTreeEditDelegate: public WindowContextAware
{
public:
    ResourceTreeEditDelegate(WindowContext* context);

    /**
     * Function call operator with the same parameters as in <tt>QAbstractItemModel::setData()</tt>
     * except <tt>role</tt>, which expected to be <tt>Qt::EditRole</tt>. Basically, it's external
     * implementation of the <tt>setData()</tt> method for the <tt>EntityItemModel</tt> based
     * Resource Tree model. <tt>EntityItemModel</tt> itself is non-specialized facade for the model
     * composition and not supposed to modify data or hold any business logic.
     */
    bool operator()(const QModelIndex& index, const QVariant& value) const;
};

} // namespace nx::vms::client::desktop

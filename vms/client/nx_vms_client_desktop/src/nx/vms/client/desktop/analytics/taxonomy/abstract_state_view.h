// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_node.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

/**
 * Represents a view of the analytics taxonomy state. It contains only such nodes (see
 * AbstractNode) and attributes (see AbstractAttribute) that should be displayed in the GUI
 * and matches corresponding filter (see AbstractStateViewFilter), which was used to build the
 * view (see AbstractStateViewBuilder). Basically, it is a set of tree-like structures with
 * containing AbstractNode items as internal nodes and AbstractAttribute items as leafs, though in
 * some cases a node can have 0 attributes and some attributes can have nested attributes (see
 * AbstractAttributeSet).
 */
class NX_VMS_CLIENT_DESKTOP_API AbstractStateView: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractNode*>
        rootNodes READ rootNodes CONSTANT)

public:
    AbstractStateView(QObject* parent): QObject(parent) {}

    virtual ~AbstractStateView() {}

    virtual std::vector<AbstractNode*> rootNodes() const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy

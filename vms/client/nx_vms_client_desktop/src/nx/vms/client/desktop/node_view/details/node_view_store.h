// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "node_view_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

class NodeViewStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    NodeViewStore(QObject* parent = nullptr);
    virtual ~NodeViewStore() override;

    const NodeViewState& state() const;

    void applyPatch(const NodeViewStatePatch& state);

signals:
    void patchApplied(const NodeViewStatePatch& patch);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

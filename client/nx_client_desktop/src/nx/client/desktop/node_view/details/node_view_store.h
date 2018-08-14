#pragma once

#include <QtCore/QObject>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

class ViewNodePath;
class NodeViewState;
class NodeViewStatePatch;

class NodeViewStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    NodeViewStore(QObject* parent = nullptr);
    virtual ~NodeViewStore() override;

    const NodeViewState& state() const;

    void applyPatch(const NodeViewStatePatch& state);

    bool applyingPatch() const;

signals:
    void patchApplied(const NodeViewStatePatch& patch);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

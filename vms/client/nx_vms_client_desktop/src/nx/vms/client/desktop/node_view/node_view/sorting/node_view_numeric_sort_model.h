#pragma once
#include "node_view_base_sort_model.h"

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewNumericSortModel: public NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

public:
    NodeViewNumericSortModel(QObject* parent = nullptr);
    virtual ~NodeViewNumericSortModel() override = default;

public:
    enum NumericMode
    {
        Lexicographic,
        Alphanumeric
    };

    void setNumericMode(NumericMode mode);
    NumericMode getNumericMode() const;

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop

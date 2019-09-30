#pragma once

#include "node_view_base_sort_model.h"
#include <QtCore/QCollator>

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewNumericSortModel: public NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

public:
    NodeViewNumericSortModel(QObject* parent = nullptr);
    virtual ~NodeViewNumericSortModel() override;

public:
    bool numericMode() const;
    void setNumericMode(bool isNumericMode);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

private:
    mutable QCollator m_collator;
};

} // namespace node_view
} // namespace nx::vms::client::desktop

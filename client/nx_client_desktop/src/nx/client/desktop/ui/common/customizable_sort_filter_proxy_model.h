#pragma once

#include <functional>

#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class CustomizableSortFilterProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    using base_type::base_type; //< forward constructors.

    // Sorting.

    using LessPredicate = std::function<bool (const QModelIndex&, const QModelIndex&)>;
    void setCustomLessThan(LessPredicate lessThan);

    virtual bool lessThan(const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    // Filtering.

    using AcceptPredicate = std::function<bool (int, const QModelIndex&)>;

    void setCustomFilterAcceptsRow(AcceptPredicate filterAcceptsRow);
    void setCustomFilterAcceptsColumn(AcceptPredicate filterAcceptsColumn);

    virtual bool filterAcceptsRow(int sourceRow,
        const QModelIndex& sourceParent) const override;
    virtual bool filterAcceptsColumn(int sourceColumn,
        const QModelIndex& sourceParent) const override;

private:
    LessPredicate m_lessThan;
    AcceptPredicate m_filterAcceptsRow;
    AcceptPredicate m_filterAcceptsColumn;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iostream>

#include <QtGui/QStandardItemModel>

namespace nx::vms::client::desktop {
namespace test {

// QStandardItemModel with a moveRows operation.
class TestItemModel: public QStandardItemModel
{
    Q_DECLARE_PRIVATE(QAbstractItemModel)

public:
    virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
        const QModelIndex& destinationParent, int destinationChild) override;

    QModelIndex buildIndex(std::initializer_list<int> indices) const;
};

} // namespace test
} // namespace nx::vms::client::desktop

// -----------------------------------------------------------------------------------------------
// Some utility functions.

inline QString toString(const QModelIndex& index)
{
    if (!index.isValid())
        return "empty";

    return index.parent().isValid()
        ? QString("%1:%2").arg(toString(index.parent()), QString::number(index.row()))
        : QString::number(index.row());
}

inline std::ostream& operator<<(std::ostream& s, const QModelIndex& index)
{
    return s << toString(index).toStdString();
}

inline std::ostream& operator<<(std::ostream& s, const QPersistentModelIndex& index)
{
    return s << toString(index).toStdString();
}

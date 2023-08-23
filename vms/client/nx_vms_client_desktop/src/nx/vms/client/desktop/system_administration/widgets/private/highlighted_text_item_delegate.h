// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include <nx/utils/impl_ptr.h>

class QTextDocument;

namespace nx::vms::client::desktop {

/**
 * Item delegate that highlights matching text if QSortFilterProxyModel is used by the view.
 */
class HighlightedTextItemDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    HighlightedTextItemDelegate(QObject* parent = nullptr, const QSet<int>& columns = {});
    virtual ~HighlightedTextItemDelegate() override;

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    // Enable monitoring of view size for cache adjustment.
    void setView(QAbstractItemView* view);

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop

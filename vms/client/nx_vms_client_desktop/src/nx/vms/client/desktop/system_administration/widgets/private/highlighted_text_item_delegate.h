// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QTextDocument;

namespace nx::vms::client::desktop {

/**
 * Item delegate that highlights matching text if QSortFilterProxyModel is used by the view.
 */
class HighlightedTextItemDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    HighlightedTextItemDelegate(QObject* parent = nullptr, const QSet<int>& colums = {});

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    void initTextDocument(QTextDocument& doc, const QStyleOptionViewItem& option) const;

protected:
    QSet<int> m_columns;
};

} // nx::vms::client::desktop

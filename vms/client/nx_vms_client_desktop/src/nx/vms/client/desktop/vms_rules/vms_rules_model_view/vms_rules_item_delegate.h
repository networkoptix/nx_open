// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    VmsRulesItemDelegate(QObject* parent = nullptr);

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    static int modificationMarkColumnWidth();

protected:

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

private:

    void drawEventRepresentation(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void drawActionRepresentation(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void drawModificationMark(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop

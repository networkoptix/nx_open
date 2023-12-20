// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class EventParametersDropDownDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    EventParametersDropDownDelegate(QObject* parent = nullptr);
    virtual ~EventParametersDropDownDelegate() override = default;

    void paint(QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
    virtual QString displayText(const QVariant& value, const QLocale& locale) const override;
};

} // namespace nx::vms::client::desktop

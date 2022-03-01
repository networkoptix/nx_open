// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "modification_mark_item_delegate.h"

#include <QtGui/QPainter>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

namespace nx::vms::client::desktop::rules {

ModificationMarkItemDelegate::ModificationMarkItemDelegate(QObject* parent):
    QStyledItemDelegate(parent)
{
}

void ModificationMarkItemDelegate::paint(QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Draw background.
    opt.features &= ~(QStyleOptionViewItem::HasDisplay
        | QStyleOptionViewItem::HasDecoration
        | QStyleOptionViewItem::HasCheckIndicator);
    Style::instance()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if (opt.checkState != Qt::Checked)
        return;

    painter->save();

    const auto textBarColor = QPalette().color(QPalette::Midlight).lighter(125);
    const auto switchHeight = style::Metrics::kStandaloneSwitchSize.height();
    QRect textBarRect = opt.rect.adjusted(8, switchHeight / 2, -8, -switchHeight / 2);

    painter->setBrush(textBarColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(textBarRect, 2, 2);

    static constexpr int kTextMargin = 2;
    const QString modificationMarkText = tr("Not saved").toUpper();
    const auto textColor = QPalette().color(QPalette::Shadow);

    auto font = opt.font;
    font.setLetterSpacing(QFont::PercentageSpacing, 110);
    font.setWeight(QFont::DemiBold);
    font.setPixelSize(8);

    painter->setFont(font);
    painter->setPen(textColor);
    painter->setRenderHint(QPainter::TextAntialiasing);

    QTextOption textOption;
    textOption.setAlignment(Qt::AlignCenter);
    painter->drawText(textBarRect, modificationMarkText, textOption);

    painter->restore();
}

QSize ModificationMarkItemDelegate::sizeHint(const QStyleOptionViewItem& option,
    const QModelIndex& /*index*/) const
{
    return QSize{option.rect.width(), style::Metrics::kStandaloneSwitchSize.height()};
}

} // namespace nx::vms::client::desktop:rules

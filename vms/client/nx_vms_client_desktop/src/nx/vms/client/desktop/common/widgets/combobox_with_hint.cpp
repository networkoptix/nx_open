// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "combobox_with_hint.h"

#include <QtWidgets/QStyledItemDelegate>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

class PaintHintItemDelegate: public QStyledItemDelegate
{
public:
    PaintHintItemDelegate(ComboBoxWithHint* owner):
        QStyledItemDelegate(owner),
        m_owner(owner)
    {
    }

protected:
    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override final
    {
        QStyledItemDelegate::paint(painter, styleOption, index);

        const auto text = index.data(m_owner->hintRole()).toString();
        if (text.isEmpty())
            return;

        auto rect = styleOption.rect;
        rect = rect.adjusted(0, 0, -nx::style::Metrics::kStandardPadding, 0);

        QnScopedPainterPenRollback penRollback(painter, painter->pen());
        painter->setPen(m_owner->hintColor());
        painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, text);
    }

private:
    ComboBoxWithHint* const m_owner;
};

ComboBoxWithHint::ComboBoxWithHint(QWidget* parent):
    base_type(parent),
    m_hintColor(colorTheme()->color("light13"))
{
    setItemDelegate(new PaintHintItemDelegate(this));
}

QSize ComboBoxWithHint::sizeHint() const
{
    QSize size = base_type::sizeHint();

    QFontMetrics metrics(font());
    int textWidth = 0;
    for (int i = 0; i < count(); ++i)
    {
        textWidth = std::max(
            textWidth,
            metrics.horizontalAdvance(itemData(i, m_hintRole).toString()));
    }

    const int spacing = nx::style::Metrics::kDefaultLayoutSpacing.width();
    size.setWidth(size.width() + textWidth + spacing);
    return size;
}

void ComboBoxWithHint::setHintColor(const QColor& color)
{
    m_hintColor = color;
}

QColor ComboBoxWithHint::hintColor() const
{
    return m_hintColor;
}

void ComboBoxWithHint::setItemHint(int index, const QString& text)
{
    setItemData(index, text, m_hintRole);
}

void ComboBoxWithHint::setHintRole(int value)
{
    m_hintRole = value;
}

int ComboBoxWithHint::hintRole() const
{
    return m_hintRole;
}

} // namespace nx::vms::client::desktop

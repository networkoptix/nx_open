#include "advanced_combo_box.h"

#include <QtWidgets/QStylePainter>

#include <ui/style/helper.h>

namespace nx {
namespace client {
namespace desktop {

AdvancedComboBox::AdvancedComboBox(QWidget* parent): base_type(parent)
{
}

AdvancedComboBox::~AdvancedComboBox()
{
}

int AdvancedComboBox::popupWidth() const
{
    return m_popupWidth;
}

void AdvancedComboBox::setPopupWidth(int value)
{
    m_popupWidth = value;
}

QString AdvancedComboBox::customText() const
{
    return m_customText;
}

void AdvancedComboBox::setCustomText(const QString& value)
{
    if (m_customText == value)
        return;

    m_customText = value;
    update();
}

int AdvancedComboBox::customTextRole() const
{
    return m_customTextRole;
}

void AdvancedComboBox::setCustomTextRole(int value)
{
    if (m_customTextRole == value)
        return;

    m_customTextRole = value;
    update();
}

Qt::TextElideMode AdvancedComboBox::textElideMode() const
{
    return m_textElideMode;
}

void AdvancedComboBox::setTextElideMode(Qt::TextElideMode value)
{
    if (m_textElideMode == value)
        return;

    m_textElideMode = value;
    update();
}

int AdvancedComboBox::effectivePopupWidth() const
{
    return m_popupWidth > 0
        ? m_popupWidth
        : base_type::sizeHint().width();
}

void AdvancedComboBox::showPopup()
{
    setProperty(style::Properties::kComboBoxPopupWidth, effectivePopupWidth());
    base_type::showPopup();
}

void AdvancedComboBox::paintEvent(QPaintEvent* event)
{
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    QStyleOptionComboBox option;
    initStyleOption(&option);
    if (!option.editable)
    {
        if (!m_customText.isNull())
            option.currentText = m_customText;
        else if (m_customTextRole >= 0)
            option.currentText = currentData(m_customTextRole).toString();
    }

    painter.drawComplexControl(QStyle::CC_ComboBox, option);

    if (m_textElideMode != Qt::ElideNone && !option.editable && !option.currentText.isEmpty())
    {
        auto editRect = style()->subControlRect(QStyle::CC_ComboBox, &option,
            QStyle::SC_ComboBoxEditField, this);

        // Few magic constants to match QCommonStyle painting...
        if (!option.currentIcon.isNull())
        {
            if (option.direction == Qt::RightToLeft)
                editRect.setRight(editRect.right() - (option.iconSize.width() + 4));
            else
                editRect.setLeft(editRect.left() + (option.iconSize.width() + 4));
        }

        editRect.adjust(2, 0, -2, 0);

        option.currentText = option.fontMetrics.elidedText(option.currentText,
            m_textElideMode, editRect.width(), Qt::TextShowMnemonic);
    }

    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

} // namespace desktop
} // namespace client
} // namespace nx

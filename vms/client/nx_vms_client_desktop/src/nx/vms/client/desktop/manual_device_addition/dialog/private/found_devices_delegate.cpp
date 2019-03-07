#include "found_devices_delegate.h"

#include <QtWidgets/QApplication>

namespace nx::vms::client::desktop {

FoundDevicesDelegate::FoundDevicesDelegate(QObject* parent):
    base_type(parent)
{
}

void FoundDevicesDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (option.state.testFlag(QStyle::State_Selected)
        && option.state.testFlag(QStyle::State_Enabled))
    {
        NX_ASSERT(index.isValid());

        QStyleOptionViewItem styleOption = option;
        initStyleOption(&styleOption, index);
        QStyle* style = styleOption.widget ? styleOption.widget->style() : QApplication::style();

        auto emptyContentsOption = styleOption;
        emptyContentsOption.text = QString();
        emptyContentsOption.icon = QIcon();
        style->drawControl(
            QStyle::CE_ItemViewItem, &emptyContentsOption, painter, emptyContentsOption.widget);

        auto noSelectionOption = styleOption;
        noSelectionOption.state.setFlag(QStyle::State_Selected, false);
        style->drawControl(
            QStyle::CE_ItemViewItem, &noSelectionOption, painter, noSelectionOption.widget);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

} // namespace nx::vms::client::desktop

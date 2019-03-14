#include "resource_node_view_item_selection_delegate.h"

#include "../resource_view_node_helpers.h"
#include "../../details/node/view_node_helpers.h"
#include "../../selection_node_view/selection_view_node_helpers.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <ui/style/helper.h>
#include <ui/style/globals.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>
#include <client/client_color_types.h>

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

ResourceNodeViewItemSelectionDelegate::ResourceNodeViewItemSelectionDelegate(
    QTreeView* owner,
    const ColumnSet& selectionColumns,
    QObject* parent)
    :
    base_type(owner, parent),
    m_selectionColumns(selectionColumns)
{
}

ResourceNodeViewItemSelectionDelegate::~ResourceNodeViewItemSelectionDelegate()
{
}

void ResourceNodeViewItemSelectionDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    const auto node = nodeFromIndex(index);
    const bool checked = std::any_of(m_selectionColumns.begin(), m_selectionColumns.end(),
        [node](int column)
        {
            return checkedState(node, column) != Qt::Unchecked;
        });

    const auto& targetColors = colors();
    const bool highlighted = checked || selectedChildrenCount(node) > 0;
    const auto extraColor = highlighted ? targetColors.extraTextSelected : targetColors.extraText;
    const auto mainColor = highlighted ? targetColors.mainTextSelected : targetColors.mainText;

    auto option = styleOption;
    initStyleOption(&option, index);
    paintItemText(painter, option, index, mainColor, extraColor, qnGlobals->errorTextColor());
    paintItemIcon(painter, option, index, highlighted ? QIcon::Selected : QIcon::Normal);
}

void ResourceNodeViewItemSelectionDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    /* Init font options: */
    option->font.setWeight(QFont::DemiBold);
    option->fontMetrics = QFontMetrics(option->font);

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
}

} // namespace node_view
} // namespace nx::vms::client::desktop


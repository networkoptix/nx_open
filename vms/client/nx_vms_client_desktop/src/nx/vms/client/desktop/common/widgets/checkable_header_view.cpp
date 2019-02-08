#include "checkable_header_view.h"

#include <QtGui/QMouseEvent>

#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

CheckableHeaderView::CheckableHeaderView(int checkBoxColumn, QWidget* parent):
    base_type(Qt::Horizontal, parent),
    m_checkBoxColumn(checkBoxColumn),
    m_checkState(Qt::Unchecked)
{
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(this, &CheckableHeaderView::sectionClicked, this, &CheckableHeaderView::at_sectionClicked);
}

Qt::CheckState CheckableHeaderView::checkState() const
{
    return m_checkState;
}

void CheckableHeaderView::setCheckState(Qt::CheckState state)
{
    if (state != m_checkState)
    {
        m_checkState = state;
        emit checkStateChanged(state);
        update();
    }
}

void CheckableHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    base_type::paintSection(painter, rect, logicalIndex);

    if (logicalIndex != m_checkBoxColumn)
        return;

    if (!rect.isValid())
        return;

    QStyleOptionViewItem opt;
    opt.initFrom(this);
    opt.displayAlignment = defaultAlignment();
    opt.features = QStyleOptionViewItem::HasCheckIndicator;
    opt.state |= QStyle::State_Raised;
    opt.state &= ~QStyle::State_HasFocus;

    if ((opt.state & QStyle::State_MouseOver) && (logicalIndexAt(mapFromGlobal(QCursor::pos())) != logicalIndex))
        opt.state &= ~QStyle::State_MouseOver;

    opt.rect = rect;
    opt.rect = style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, this);

    switch(m_checkState)
    {
    case Qt::Checked:
        opt.state |= QStyle::State_On;
        break;
    case Qt::Unchecked:
        opt.state |= QStyle::State_Off;
        break;
    default:
        opt.state |= QStyle::State_NoChange;
        break;
    }

    QnScopedPainterClipRegionRollback clipRollback(painter, QRegion(rect));
    style()->drawPrimitive(QStyle::PE_IndicatorViewItemCheck, &opt, painter, this);
}

QSize CheckableHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    if (logicalIndex == m_checkBoxColumn)
    {
        QStyleOptionViewItem opt;
        opt.initFrom(this);
        opt.displayAlignment = defaultAlignment();
        opt.features = QStyleOptionViewItem::HasCheckIndicator;
        return style()->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), this);
    }

    return base_type::sectionSizeFromContents(logicalIndex);
}

void CheckableHeaderView::at_sectionClicked(int logicalIndex)
{
    if (logicalIndex == m_checkBoxColumn)
        setCheckState(m_checkState == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

void CheckableHeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    int pos = orientation() == Qt::Horizontal ? event->x() : event->y();
    int section = logicalIndexAt(pos);
    if (section == m_checkBoxColumn)
    {
        emit sectionClicked(section);
    }
    else
    {
        base_type::mouseReleaseEvent(event);
    }
}

void CheckableHeaderView::mousePressEvent(QMouseEvent* event)
{
    int pos = orientation() == Qt::Horizontal ? event->x() : event->y();
    int section = logicalIndexAt(pos);
    if (section != m_checkBoxColumn)
        base_type::mousePressEvent(event);
}

} // namespace nx::vms::client::desktop

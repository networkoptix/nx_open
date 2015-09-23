#include "checkboxed_header_view.h"

QnCheckBoxedHeaderView::QnCheckBoxedHeaderView(int checkBoxColumn, QWidget *parent):
    base_type(Qt::Horizontal, parent),
    m_checkBoxColumn(checkBoxColumn),
    m_checkState(Qt::Unchecked)
{
    connect(this, &QnCheckBoxedHeaderView::sectionClicked, this, &QnCheckBoxedHeaderView::at_sectionClicked);
}

Qt::CheckState QnCheckBoxedHeaderView::checkState() const {
    return m_checkState;
}

void QnCheckBoxedHeaderView::setCheckState(Qt::CheckState state) {
    if (state == m_checkState)
        return;
    m_checkState = state;
    emit checkStateChanged(state);
    update();
}

void QnCheckBoxedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    base_type::paintSection(painter, rect, logicalIndex);

    if (logicalIndex == m_checkBoxColumn) {
        if (!rect.isValid())
            return;
        QStyleOptionButton opt;
        opt.initFrom(this);

        QStyle::State state = QStyle::State_Raised;
        if (isEnabled())
            state |= QStyle::State_Enabled;
        if (window()->isActiveWindow())
            state |= QStyle::State_Active;

        switch(m_checkState) {
        case Qt::Checked:
            state |= QStyle::State_On;
            break;
        case Qt::Unchecked:
            state |= QStyle::State_Off;
            break;
        default:
            state |= QStyle::State_NoChange;
            break;
        }

        opt.rect = rect.adjusted(4, 0, 0, 0);
        opt.state |= state;
        opt.text = QString();
        style()->drawControl(QStyle::CE_CheckBox, &opt, painter, this);
        return;
    }
}

QSize QnCheckBoxedHeaderView::sectionSizeFromContents(int logicalIndex) const {
    QSize size = base_type::sectionSizeFromContents(logicalIndex);
    if (logicalIndex != m_checkBoxColumn)
        return size;
    size.setWidth(15);
    return size;
}

void QnCheckBoxedHeaderView::at_sectionClicked(int logicalIndex) {
    if (logicalIndex != m_checkBoxColumn)
        return;
    if (m_checkState != Qt::Checked)
        setCheckState(Qt::Checked);
    else
        setCheckState(Qt::Unchecked);
}

void QnCheckBoxedHeaderView::mouseReleaseEvent(QMouseEvent *e)
{
    int pos = orientation() == Qt::Horizontal ? e->x() : e->y();
    int section = logicalIndexAt(pos);
    if (section == 0) {
        emit sectionClicked(section);
    }
    else {
        base_type::mouseReleaseEvent(e);
    }
}

void QnCheckBoxedHeaderView::mousePressEvent(QMouseEvent *e)
{
    int pos = orientation() == Qt::Horizontal ? e->x() : e->y();
    int section = logicalIndexAt(pos);
    if (section != 0)
        base_type::mousePressEvent(e);
}

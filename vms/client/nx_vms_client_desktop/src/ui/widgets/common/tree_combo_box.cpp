// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tree_combo_box.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>

#include <nx/utils/guarded_callback.h>
#include <utils/common/delayed.h>

namespace {

    QModelIndex indexUp(const QModelIndex &idx) {
        if (!idx.isValid())
            return QModelIndex();
        if (idx.row() > 0) {
            QModelIndex up = idx.sibling(idx.row() - 1, idx.column());
            if (idx.model()->rowCount(up) > 0)
                return idx.model()->index(idx.model()->rowCount(up) -  1, idx.column(), up);
            return up;
        }
        return idx.parent();
    }

    QModelIndex parentDown(const QModelIndex &idx) {
        if (!idx.isValid())
            return QModelIndex();
        QModelIndex parent = idx.parent();
        if (idx.model()->rowCount(parent) > idx.row() + 1)
            return idx.sibling(idx.row() + 1, idx.column());
        return parentDown(idx.parent());
    }

    QModelIndex indexDown(const QModelIndex &idx) {
        if (!idx.isValid())
            return QModelIndex();
        if (idx.model()->rowCount(idx) > 0)
            return idx.model()->index(0, idx.column(), idx);
        return parentDown(idx);
    }

}

QnTreeComboBox::QnTreeComboBox(QWidget *parent):
    base_type(parent),
    m_treeView(new QTreeView(this))
{
    m_treeView->setFrameShape(QFrame::NoFrame);
    m_treeView->setEditTriggers(QTreeView::NoEditTriggers);
    m_treeView->setSelectionBehavior(QTreeView::SelectRows);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setWordWrap(true);
    m_treeView->setAllColumnsShowFocus(true);
    m_treeView->header()->setVisible(false);

    m_treeView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    setView(m_treeView);

    setSizeAdjustPolicy(AdjustToContents);

    connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(at_currentIndexChanged(int)));

    m_updateHighlight = new nx::utils::PendingOperation(this);
    m_updateHighlight->setCallback([this] { updateHighlight(); });
    m_updateHighlight->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
}

QModelIndex QnTreeComboBox::currentHighlightedIndex() const
{
    return m_treeView->currentIndex();
}

QModelIndex QnTreeComboBox::currentModelIndex() const
{
    return m_currentRoot.isValid()
        ? model()->index(base_type::currentIndex(), modelColumn(), m_currentRoot)
        : QModelIndex();
}

void QnTreeComboBox::setResizeViewToContents(bool enabled)
{
    m_resizeViewToContents = enabled;
}

bool QnTreeComboBox::resizeViewToContents() const
{
    return m_resizeViewToContents;
}

void QnTreeComboBox::showPopup() {
    setRootModelIndex(QModelIndex());

    for(int i = 1; i<model()->columnCount(); ++i)
        m_treeView->hideColumn(i);

    m_treeView->expandAll();
    m_treeView->setItemsExpandable(false);

    if (m_resizeViewToContents)
        view()->setMinimumWidth(minimumSizeHint().width());

    base_type::showPopup();
}

void QnTreeComboBox::setModel(QAbstractItemModel* model)
{
    disconnect(m_expandConnection);
    base_type::setModel(model);
    m_expandConnection = connect(model, &QAbstractItemModel::rowsInserted,
        m_treeView,
        [this, model](const QModelIndex& parent, int first, int last)
        {
            for (int row = first; row <= last; ++row)
                m_treeView->expand(model->index(row, modelColumn(), parent));

            m_updateHighlight->requestOperation();
        });
}

void QnTreeComboBox::updateHighlight()
{
    if (!m_treeView->isVisible() || !m_treeView->underMouse())
        return;

    auto highlightIndex = m_treeView->indexAt(m_treeView->mapFromGlobal(QCursor::pos()));

    if (highlightIndex.isValid())
        m_treeView->setCurrentIndex(highlightIndex);
}

void QnTreeComboBox::setCurrentIndex(const QModelIndex& index) {
    if (!index.isValid()) {
        base_type::setCurrentIndex(0);
        return;
    }

    setRootModelIndex(index.parent());
    base_type::setCurrentIndex(index.row());
    setRootModelIndex(QModelIndex());
    m_currentRoot = index.parent();

    if (!m_treeView->isVisible())
        m_treeView->setCurrentIndex(index);
}

int QnTreeComboBox::getTreeWidth(const QModelIndex& parent, int nestedLevel) const {
    const QFontMetrics &fm = fontMetrics();
    int textWidth = 7 * fm.horizontalAdvance(QLatin1Char('x'));

    for (int i = 0; i < model()->rowCount(parent); ++i)
    {
        QModelIndex idx = model()->index(i, 0, parent);

        QString itemText = model()->data(idx).toString();
        bool hasIcon = !model()->data(idx, Qt::DecorationRole).isNull();
        int iconWidth = hasIcon ? iconSize().width() + 4 : 0;
        int w = fm.boundingRect(itemText).width() + iconWidth + nestedLevel * m_treeView->indentation();
        if (nestedLevel > 0)
            w += 4; // I don't know why. But if nested level is exists addition size is required.
        textWidth = qMax(textWidth, w);
        textWidth = qMax(textWidth, getTreeWidth(idx, nestedLevel+1));
    }
    return textWidth;
}

QModelIndex QnTreeComboBox::findData(const QVariant& data, int role, Qt::MatchFlags flags) const
{
    const QModelIndex start = model()->index(0, modelColumn(), rootModelIndex());
    const QModelIndexList result = model()->match(start, role, data, 1, flags);
    return result.empty() ? QModelIndex() : result.front();
}

QSize QnTreeComboBox::minimumSizeHint() const
{
    switch (sizeAdjustPolicy())
    {
        case AdjustToContents:
            break;

        case AdjustToContentsOnFirstShow:
            if (m_minimumSizeHint.isValid())
                return m_minimumSizeHint;
            break;

        default:
            return base_type::minimumSizeHint();
    }

    int textWidth = getTreeWidth(QModelIndex(), 0);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    QSize tmp(textWidth, 0);
    tmp = style()->sizeFromContents(QStyle::CT_ComboBox, &opt, tmp, this);
    textWidth = tmp.width();

    QSize sz = base_type::sizeHint();
    m_minimumSizeHint = QSize(textWidth, sz.height());
    return m_minimumSizeHint;
}

void QnTreeComboBox::wheelEvent(QWheelEvent *e) {
    QModelIndex newIndex;
    if (e->angleDelta().y() > 0) {
        newIndex = indexUp(currentHighlightedIndex());
        while(newIndex.isValid() &&
              (newIndex.model()->flags(newIndex) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) )
              != (Qt::ItemIsEnabled | Qt::ItemIsSelectable))
            newIndex = indexUp(newIndex);
    } else {
        newIndex = indexDown(currentHighlightedIndex());
        while(newIndex.isValid() &&
              (newIndex.model()->flags(newIndex) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) )
              != (Qt::ItemIsEnabled | Qt::ItemIsSelectable))
            newIndex = indexDown(newIndex);
    }

    if (newIndex.isValid() && newIndex != currentHighlightedIndex()) {
        setCurrentIndex(newIndex);
    }

    e->accept();
}

void QnTreeComboBox::keyPressEvent(QKeyEvent *e) {
    enum Move { NoMove=0 , MoveUp , MoveDown , MoveFirst , MoveLast};

    Move move = NoMove;

    switch (e->key()) {
    case Qt::Key_Up:
    case Qt::Key_PageUp:
        move = MoveUp;
        break;
    case Qt::Key_Down:
        if (e->modifiers() & Qt::AltModifier) {
            showPopup();
            return;
        }
        move = MoveDown;
        break;
    case Qt::Key_PageDown:
        move = MoveDown;
        break;
    case Qt::Key_Home:
        move = MoveFirst;
        break;
    case Qt::Key_End:
        move = MoveLast;
        break;

    default:
        base_type::keyPressEvent(e);
        return;
    }

    if (move == NoMove) {
        base_type::keyPressEvent(e);
        return;
    }

    QModelIndex newIndex;

    e->accept();
    switch (move) {
    case MoveFirst:
    case MoveUp:
        newIndex = indexUp(currentHighlightedIndex());
        while(newIndex.isValid() &&
              (newIndex.model()->flags(newIndex) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) )
              != (Qt::ItemIsEnabled | Qt::ItemIsSelectable))
            newIndex = indexUp(newIndex);
        break;
    case MoveDown:
    case MoveLast:
        newIndex = indexDown(currentHighlightedIndex());
        while(newIndex.isValid() &&
              (newIndex.model()->flags(newIndex) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) )
              != (Qt::ItemIsEnabled | Qt::ItemIsSelectable))
            newIndex = indexDown(newIndex);
        break;
    default:
        e->ignore();
        break;
    }

    if (newIndex.isValid() && newIndex != currentHighlightedIndex()) {
        setCurrentIndex(newIndex);
    }


}

void QnTreeComboBox::at_currentIndexChanged(int index) {
    Q_UNUSED(index);
    m_currentRoot = m_treeView->currentIndex().parent();
    emit currentIndexChanged(currentHighlightedIndex());
}

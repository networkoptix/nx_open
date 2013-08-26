#include "tree_combo_box.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>

QnTreeComboBox::QnTreeComboBox(QWidget *parent):
    base_type(parent),
    m_treeView(new QTreeView(this))
{
    m_treeView->setFrameShape(QFrame::NoFrame);
    m_treeView->setEditTriggers(QTreeView::NoEditTriggers);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionBehavior(QTreeView::SelectRows);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setWordWrap(true);
    m_treeView->setAllColumnsShowFocus(true);
    m_treeView->header()->setVisible(false);

    m_treeView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    setView(m_treeView);

    connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(at_currentIndexChanged(int)));
}

QModelIndex QnTreeComboBox::currentIndex() const {
    return m_treeView->currentIndex();
}

void QnTreeComboBox::showPopup() {
    setRootModelIndex(QModelIndex());

    for(int i = 1; i<model()->columnCount(); ++i)
        m_treeView->hideColumn(i);

    m_treeView->expandAll();
    m_treeView->setItemsExpandable(false);
    base_type::showPopup();
}

void QnTreeComboBox::setCurrentIndex(const QModelIndex& index) {
    if (!index.isValid()) {
        base_type::setCurrentIndex(0);
        return;
    }

    setRootModelIndex(index.parent());
    base_type::setCurrentIndex(index.row());
    m_treeView->setCurrentIndex(index);
}

int QnTreeComboBox::getTreeWidth(const QModelIndex& parent, int nestedLevel) const {
    const QFontMetrics &fm = fontMetrics();
    int textWidth = 7 * fm.width(QLatin1Char('x'));

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

QSize QnTreeComboBox::minimumSizeHint() const {
    int textWidth = getTreeWidth(QModelIndex(), 0);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    QSize tmp(textWidth, 0);
    tmp = style()->sizeFromContents(QStyle::CT_ComboBox, &opt, tmp, this);
    textWidth = tmp.width();

    QSize sz = base_type::sizeHint();
    return QSize(textWidth, sz.height()).expandedTo(QApplication::globalStrut());
}

void QnTreeComboBox::at_currentIndexChanged(int index) {
    Q_UNUSED(index);
    emit currentIndexChanged(currentIndex());
}

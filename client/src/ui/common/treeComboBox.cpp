#include "treeComboBox.h"

#include <QtGui/QHeaderView>

QnTreeViewComboBox::QnTreeViewComboBox(QWidget *parent): QComboBox(parent), m_treeView(NULL) 
{
    m_treeView = new QTreeView(this);
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
}

void QnTreeViewComboBox::showPopup() 
{
    setRootModelIndex(QModelIndex());

    for(int i=1;i<model()->columnCount();++i)
        m_treeView->hideColumn(i);

    m_treeView->expandAll();
    m_treeView->setItemsExpandable(false);
    QComboBox::showPopup();
}

void QnTreeViewComboBox::selectIndex(const QModelIndex& index) 
{
    setRootModelIndex(index.parent());
    setCurrentIndex(index.row());
}

int QnTreeViewComboBox::getTreeWidth(const QModelIndex& parent, int nestedLevel) const
{
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

QSize QnTreeViewComboBox::sizeHint() const
{   
    int textWidth = getTreeWidth(QModelIndex(), 0);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    QSize tmp(textWidth, 0);
    tmp = style()->sizeFromContents(QStyle::CT_ComboBox, &opt, tmp, this);
    textWidth = tmp.width();

    QSize sz = QComboBox::sizeHint();
    return QSize(textWidth, sz.height()).expandedTo(QApplication::globalStrut());
}

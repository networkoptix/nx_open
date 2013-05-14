#ifndef TREEVIEWCOMBOBOX_H
#define TREEVIEWCOMBOBOX_H

#include <QtGui/QComboBox>
#include <QtGui/QTreeView>

class QnTreeViewComboBox : public QComboBox 
{
    Q_OBJECT
public:
    explicit QnTreeViewComboBox(QWidget *parent = 0);
    virtual void showPopup();

    QModelIndex currentIndex();
protected:
    virtual QSize sizeHint() const override;
public slots:
    void selectIndex(const QModelIndex&);
private:
    QSize recomputeSizeHint(QSize &sh) const;
    int getTreeWidth(const QModelIndex& parent, int nestedLevel) const;
private:
    QTreeView* m_treeView;
};

#endif // TREEVIEWCOMBOBOX_H

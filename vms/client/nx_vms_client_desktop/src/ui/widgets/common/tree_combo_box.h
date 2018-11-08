#ifndef TREE_COMBO_BOX_H
#define TREE_COMBO_BOX_H

#include <QtWidgets/QComboBox>
#include <QtWidgets/QTreeView>

class QnTreeComboBox : public QComboBox
{
    Q_OBJECT

    typedef QComboBox base_type;
public:
    explicit QnTreeComboBox(QWidget *parent = 0);
    virtual void showPopup();

    QModelIndex currentIndex() const;
protected:
    virtual QSize minimumSizeHint() const override;

    virtual void wheelEvent(QWheelEvent *e) override;
    virtual void keyPressEvent(QKeyEvent *e) override;
public slots:
    void setCurrentIndex(const QModelIndex &index);
signals:
    void currentIndexChanged(const QModelIndex &index);
private:
    QSize recomputeSizeHint(QSize &sh) const;
    int getTreeWidth(const QModelIndex& parent, int nestedLevel) const;
    Q_SLOT void at_currentIndexChanged(int index);
private:
    QTreeView* m_treeView;
    mutable QSize m_minimumSizeHint;
};

#endif // TREE_COMBO_BOX_H

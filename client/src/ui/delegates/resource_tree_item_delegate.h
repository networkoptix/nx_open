#ifndef QN_RESOURCE_TREE_ITEM_DELEGATE_H
#define QN_RESOURCE_TREE_ITEM_DELEGATE_H

#include <QtCore/QPointer>
#include <QtWidgets/QStyledItemDelegate>

class QnWorkbench;

class QnResourceTreeItemDelegate: public QStyledItemDelegate {
    typedef QStyledItemDelegate base_type;

public:
    explicit QnResourceTreeItemDelegate(QObject *parent = NULL);

    QnWorkbench *workbench() const {
        return m_workbench.data();
    }

    void setWorkbench(QnWorkbench *workbench) {
        m_workbench = workbench;
    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    
    virtual void destroyEditor(QWidget *editor, const QModelIndex &index) const override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    QPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon, m_scheduledIcon, m_buggyIcon;
};

#endif // QN_RESOURCE_TREE_ITEM_DELEGATE_H

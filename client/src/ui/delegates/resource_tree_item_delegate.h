#ifndef QN_RESOURCE_TREE_ITEM_DELEGATE_H
#define QN_RESOURCE_TREE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

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

private:
    QWeakPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon, m_scheduledIcon, m_raisedIcon, m_buggyIcon;
};

#endif // QN_RESOURCE_TREE_ITEM_DELEGATE_H

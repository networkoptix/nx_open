#ifndef QN_RESOURCE_TREE_ITEM_DELEGATE_H
#define QN_RESOURCE_TREE_ITEM_DELEGATE_H

#include <QtCore/QPointer>
#include <QtWidgets/QStyledItemDelegate>

class QnWorkbench;

class QnResourceTreeItemDelegate : public QStyledItemDelegate
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnResourceTreeItemDelegate(QObject* parent = nullptr);

    QnWorkbench* workbench() const { return m_workbench.data(); }
    void setWorkbench(QnWorkbench* workbench) { m_workbench = workbench; }

protected:
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const override;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    enum class ItemState
    {
        Normal,
        Selected,
        Accented
    };

    ItemState itemState(const QModelIndex& index) const;

private:
    QPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon, m_scheduledIcon, m_buggyIcon;
};

#endif // QN_RESOURCE_TREE_ITEM_DELEGATE_H

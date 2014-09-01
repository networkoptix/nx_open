#ifndef UPDATE_STATUS_ITEM_DELEGATE_H
#define UPDATE_STATUS_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class QnUpdateStatusItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnUpdateStatusItemDelegate(QWidget *parent = 0);
    ~QnUpdateStatusItemDelegate();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};

#endif // UPDATE_STATUS_ITEM_DELEGATE_H

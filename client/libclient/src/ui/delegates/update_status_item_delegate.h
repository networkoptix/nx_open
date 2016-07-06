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

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    void paintProgressBar(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // UPDATE_STATUS_ITEM_DELEGATE_H

#ifndef PTZ_TOUR_LIST_ITEM_DELEGATE_H
#define PTZ_TOUR_LIST_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class QnPtzTourListItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnPtzTourListItemDelegate(QObject *parent = 0);
    ~QnPtzTourListItemDelegate();

protected:
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

};

#endif // PTZ_TOUR_LIST_ITEM_DELEGATE_H


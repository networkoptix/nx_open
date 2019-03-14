#ifndef PTZ_TOUR_ITEM_DELEGATE_H
#define PTZ_TOUR_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class QnPtzTourItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnPtzTourItemDelegate(QObject *parent = 0);
    ~QnPtzTourItemDelegate();
protected:
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void at_editor_commit();
};

#endif // PTZ_TOUR_ITEM_DELEGATE_H

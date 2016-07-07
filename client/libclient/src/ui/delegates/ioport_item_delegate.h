#ifndef IOPORT_ITEM_DELEGATE_H
#define IOPORT_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QPushButton>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

typedef QVector<QnUuid> IDList;

class QnIOPortItemDelegate: public QStyledItemDelegate 
{
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnIOPortItemDelegate(QObject *parent = 0);
    ~QnIOPortItemDelegate();
protected:
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;
private slots:
    void at_editor_commit();
};

#endif // IOPORT

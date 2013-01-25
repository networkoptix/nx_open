#ifndef BUSINESS_RULE_ITEM_DELEGATE_H
#define BUSINESS_RULE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>
#include <QtGui/QPushButton>

#include <core/resource/resource_fwd.h>

class QnIndexedDialogButton: public QPushButton {
    Q_OBJECT

public:
    explicit QnIndexedDialogButton(QWidget *parent=0);

    QnResourceList resources();
    void setResources(QnResourceList resources);

private slots:
    void at_clicked();
private:
    QnResourceList m_resources;
};

class QnBusinessRuleItemDelegate: public QStyledItemDelegate {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
private:
    int m_editingRow;
    int m_editingColumn;

};

#endif // BUSINESS_RULE_ITEM_DELEGATE_H

#ifndef BUSINESS_RULE_ITEM_DELEGATE_H
#define BUSINESS_RULE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

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

private slots:
    void at_sourceButton_clicked();

private:
    int m_editingRow;
    int m_editingColumn;

};

#endif // BUSINESS_RULE_ITEM_DELEGATE_H

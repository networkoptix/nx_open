#ifndef BUSINESS_RULE_ITEM_DELEGATE_H
#define BUSINESS_RULE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>
#include <QtGui/QPushButton>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/resource_selection_dialog.h>

class QnSelectResourcesDialogButton: public QPushButton {
    Q_OBJECT

    typedef QPushButton base_type;

public:
    explicit QnSelectResourcesDialogButton(QWidget* parent=NULL);

    QnResourceList resources() const;
    void setResources(QnResourceList resources);

    QnResourceSelectionDialogDelegate* dialogDelegate() const;
    void setDialogDelegate(QnResourceSelectionDialogDelegate* delegate);

    Qn::NodeType nodeType() const;
    void setNodeType(Qn::NodeType nodeType);
signals:
    void commit();

protected:
    void initStyleOption(QStyleOptionButton *option) const;
    void paintEvent(QPaintEvent *event);

private slots:
    void at_clicked();
private:
    QnResourceList m_resources;
    QnResourceSelectionDialogDelegate* m_dialogDelegate;
    Qn::NodeType m_nodeType;
};

class QnBusinessRuleItemDelegate: public QStyledItemDelegate {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;

protected:
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private slots:
    void at_editor_commit();

};

#endif // BUSINESS_RULE_ITEM_DELEGATE_H

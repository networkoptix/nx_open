#ifndef BUSINESS_RULE_ITEM_DELEGATE_H
#define BUSINESS_RULE_ITEM_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QPushButton>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QnSelectResourcesDialogButton: public QPushButton {
    Q_OBJECT

    typedef QPushButton base_type;

public:
    explicit QnSelectResourcesDialogButton(QWidget* parent=NULL);

    QnResourceList resources() const;
    void setResources(QnResourceList resources);

    QnResourceSelectionDialogDelegate* dialogDelegate() const;
    void setDialogDelegate(QnResourceSelectionDialogDelegate* delegate);

    QnResourceSelectionDialog::SelectionTarget selectionTarget() const;
    void setSelectionTarget(QnResourceSelectionDialog::SelectionTarget target);
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
    QnResourceSelectionDialog::SelectionTarget m_target;
};

class QnBusinessRuleItemDelegate: public QStyledItemDelegate, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnBusinessRuleItemDelegate(QObject *parent = 0);
    ~QnBusinessRuleItemDelegate();

    static int optimalWidth(int column, const QFontMetrics &metrics);
protected:
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;
private slots:
    void at_editor_commit();
};

#endif // BUSINESS_RULE_ITEM_DELEGATE_H

#pragma once

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QPushButton>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

typedef QVector<QnUuid> IDList;
class QnBusinessTypesComparator;

class QnSelectResourcesDialogButton: public QPushButton {
    Q_OBJECT

    typedef QPushButton base_type;

public:
    explicit QnSelectResourcesDialogButton(QWidget* parent=NULL);

    QSet<QnUuid> resources() const;
    void setResources(QSet<QnUuid> resources);

    QnResourceSelectionDialogDelegate* dialogDelegate() const;
    void setDialogDelegate(QnResourceSelectionDialogDelegate* delegate);

    QnResourceSelectionDialog::Filter selectionTarget() const;
    void setSelectionTarget(QnResourceSelectionDialog::Filter target);
signals:
    void commit();

protected:
    void initStyleOption(QStyleOptionButton *option) const;
    void paintEvent(QPaintEvent *event);

private slots:
    void at_clicked();
private:
    QSet<QnUuid> m_resources;
    QnResourceSelectionDialogDelegate* m_dialogDelegate;
    QnResourceSelectionDialog::Filter m_target;
};

class QnBusinessRuleItemDelegate: public QStyledItemDelegate, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QStyledItemDelegate base_type;
public:
    explicit QnBusinessRuleItemDelegate(QObject *parent = 0);
    ~QnBusinessRuleItemDelegate();

    static int optimalWidth(int column, const QFontMetrics &metrics);

    virtual void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    virtual QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;
private slots:
    void at_editor_commit();

private:
    QScopedPointer<QnBusinessTypesComparator> m_lexComparator;
};

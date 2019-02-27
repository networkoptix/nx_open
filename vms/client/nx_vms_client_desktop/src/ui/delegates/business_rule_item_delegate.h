#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include <ui/models/business_rule_view_model.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBusinessTypesComparator;

namespace nx { namespace vms { namespace event { class QnBusinessStringsHelper; }}}

class QnBusinessRuleItemDelegate: public QStyledItemDelegate, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    explicit QnBusinessRuleItemDelegate(QObject* parent = nullptr);
    virtual ~QnBusinessRuleItemDelegate() override;

    using Column = QnBusinessRuleViewModel::Column;
    static int optimalWidth(Column column, const QFontMetrics& metrics);

    virtual void updateEditorGeometry(
        QWidget* editor,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    virtual QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    virtual void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index) const override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    QWidget* createSourceEditor(QWidget* parent, const QModelIndex& index) const;
    QWidget* createTargetEditor(QWidget* parent, const QModelIndex& index) const;
    QWidget* createEventEditor(QWidget* parent, const QModelIndex& index) const;
    QWidget* createActionEditor(QWidget* parent, const QModelIndex& index) const;
    QWidget* createAggregationEditor(QWidget* parent, const QModelIndex& index) const;
    void emitCommitData();

private:
    QScopedPointer<QnBusinessTypesComparator> m_lexComparator;
    QScopedPointer<nx::vms::event::StringsHelper> m_businessStringsHelper;
};

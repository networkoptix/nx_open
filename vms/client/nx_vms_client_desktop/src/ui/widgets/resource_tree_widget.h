// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndexList>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

class QTreeView;
class QAbstractItemModel;
class QItemSelectionModel;

class QnResourceItemDelegate;
class QnResourceSearchProxyModel;
class QnWorkbench;
class QnResourceTreeSortProxyModel;

namespace nx::vms::client::desktop::ResourceTree { enum class ActivationType; }

namespace Ui { class QnResourceTreeWidget; }

class QnResourceTreeWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnResourceTreeWidget(QWidget* parent = nullptr);
    virtual ~QnResourceTreeWidget() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    QnResourceSearchProxyModel* searchModel() const;
    QItemSelectionModel* selectionModel();

    void setWorkbench(QnWorkbench* workbench);

    void edit();

    QPoint selectionPos() const;

    /**
     * Allow some nodes to be auto-expanded. By default all nodes are auto-expanded.
     */
    using AutoExpandPolicy = std::function<bool(const QModelIndex&)>;
    void setAutoExpandPolicy(AutoExpandPolicy policy);

    QTreeView* treeView() const;
    QnResourceItemDelegate* itemDelegate() const;

    using base_type::update;
    void update(const QnResourcePtr& resource);

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void activated(const QModelIndex& index,
        const QModelIndexList& selection,
        nx::vms::client::desktop::ResourceTree::ActivationType activationType,
        const Qt::KeyboardModifiers modifiers);

private:
    void at_resourceProxyModel_rowsInserted(const QModelIndex& parent, int start, int end);
    void at_resourceProxyModel_modelReset();
    void expandNodeIfNeeded(const QModelIndex& index);

    void updateColumns();

private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;
    QnResourceItemDelegate* m_itemDelegate;
    AutoExpandPolicy m_autoExpandPolicy;
    QnResourceSearchProxyModel* m_resourceProxyModel;
};

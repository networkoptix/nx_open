// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_widget.h"
#include "ui_resource_tree_widget.h"

#include <QtGui/QContextMenuEvent>
#include <QtCore/QItemSelectionModel>
#include <QtWidgets/QScrollBar>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource_search_proxy_model.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

#include <ui/common/indents.h>
#include <ui/common/palette.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <client/client_globals.h>
#include <core/resource/resource.h>

using namespace nx::vms::client::desktop;

QnResourceTreeWidget::QnResourceTreeWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::QnResourceTreeWidget()),
    m_itemDelegate(new QnResourceItemDelegate(this)),
    m_resourceProxyModel(nullptr)
{
    ui->setupUi(this);

    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark8", 102));
    setPaletteColor(this, QPalette::Highlight, colorTheme()->color("brand_core", 77));

    setPaletteColor(this, QPalette::Inactive, QPalette::Midlight,
        colorTheme()->color("dark8", 102));
    setPaletteColor(this, QPalette::Inactive, QPalette::Highlight,
        colorTheme()->color("dark8", 102));

    m_itemDelegate->setFixedHeight(0); // automatic height

    ui->resourcesTreeView->setPalette(palette()); //< Override default item view palette.
    ui->resourcesTreeView->setItemDelegateForColumn(Qn::NameColumn, m_itemDelegate);
    ui->resourcesTreeView->setProperty(nx::style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(0, 1)));
    ui->resourcesTreeView->setIndentation(nx::style::Metrics::kDefaultIconSize);
    ui->resourcesTreeView->setAcceptDrops(true);
    ui->resourcesTreeView->setDragDropMode(QAbstractItemView::DragDrop);
    ui->resourcesTreeView->setEditTriggers(
        QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    ui->resourcesTreeView->setIgnoreWheelEvents(true);

    ui->resourcesTreeView->setConfirmExpandDelegate(
        [](const QModelIndex& index) -> bool
        {
            // Layouts must not expand on double click
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            return !resource || !resource->hasFlags(Qn::layout);
        });

    connect(ui->resourcesTreeView,
        &TreeView::enterPressed,
        this,
        [this](const QModelIndex& index, const Qt::KeyboardModifiers modifiers)
        {
            emit activated(index,
                selectionModel()->selectedRows(),
                ResourceTree::ActivationType::enterKey,
                modifiers);
        });
    connect(ui->resourcesTreeView,
        &TreeView::doubleClicked,
        this,
        [this](const QModelIndex& index)
        {
            emit activated(index,
                {},
                ResourceTree::ActivationType::doubleClick,
                Qt::NoModifier);
        });

    ui->resourcesTreeView->installEventFilter(this);
}

QnResourceTreeWidget::~QnResourceTreeWidget()
{
    setWorkbench(nullptr);
}

QAbstractItemModel* QnResourceTreeWidget::model() const
{
    return m_resourceProxyModel
        ? m_resourceProxyModel->sourceModel()
        : nullptr;
}

void QnResourceTreeWidget::setModel(QAbstractItemModel* model)
{
    if (m_resourceProxyModel)
    {
        m_resourceProxyModel->disconnect(this);
        delete m_resourceProxyModel;
        m_resourceProxyModel = nullptr;
    }

    if (model)
    {
        m_resourceProxyModel = new QnResourceSearchProxyModel(this);
        m_resourceProxyModel->setSourceModel(model);

        ui->resourcesTreeView->setModel(m_resourceProxyModel);

        connect(m_resourceProxyModel, &QAbstractItemModel::rowsInserted, this,
            &QnResourceTreeWidget::at_resourceProxyModel_rowsInserted);
        connect(m_resourceProxyModel, &QAbstractItemModel::modelReset, this,
            &QnResourceTreeWidget::at_resourceProxyModel_modelReset);
        expandNodeIfNeeded(QModelIndex());

        updateColumns();
    }
    else
    {
        ui->resourcesTreeView->setModel(nullptr);
    }
}

QnResourceSearchProxyModel* QnResourceTreeWidget::searchModel() const
{
    return m_resourceProxyModel;
}

QItemSelectionModel* QnResourceTreeWidget::selectionModel()
{
    return ui->resourcesTreeView->selectionModel();
}

void QnResourceTreeWidget::setWorkbench(QnWorkbench* workbench)
{
    m_itemDelegate->setWorkbench(workbench);
}

void QnResourceTreeWidget::edit()
{
    QAbstractItemView* view = ui->resourcesTreeView;
    view->edit(selectionModel()->currentIndex());
}

QPoint QnResourceTreeWidget::selectionPos() const
{
    QModelIndexList selectedRows = ui->resourcesTreeView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return QPoint();

    QModelIndex selected = selectedRows.back();
    QPoint pos = ui->resourcesTreeView->visualRect(selected).bottomRight();
    return QnHiDpiWorkarounds::safeMapToGlobal(ui->resourcesTreeView, pos);
}

void QnResourceTreeWidget::setAutoExpandPolicy(AutoExpandPolicy policy)
{
    m_autoExpandPolicy = policy;
}

QTreeView* QnResourceTreeWidget::treeView() const
{
    return ui->resourcesTreeView;
}

QnResourceItemDelegate* QnResourceTreeWidget::itemDelegate() const
{
    return m_itemDelegate;
}

void QnResourceTreeWidget::updateColumns()
{
    if (!model())
        return;

    ui->resourcesTreeView->header()->setStretchLastSection(false);
    ui->resourcesTreeView->header()->setSectionResizeMode(Qn::NameColumn, QHeaderView::Stretch);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnResourceTreeWidget::eventFilter(QObject* obj, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ContextMenu:
            if (obj == ui->resourcesTreeView)
            {
                QContextMenuEvent* me = static_cast<QContextMenuEvent*>(event);
                if (me->reason() == QContextMenuEvent::Mouse && !ui->resourcesTreeView->indexAt(me->pos()).isValid())
                    selectionModel()->clear();
            }
            break;

        case QEvent::PaletteChange:
            ui->resourcesTreeView->setPalette(palette()); // override default item view palette
            break;

        default:
            break;
    }

    return base_type::eventFilter(obj, event);
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex& parent,
    int start, int end)
{
    for (int i = start; i <= end; i++)
        expandNodeIfNeeded(m_resourceProxyModel->index(i, 0, parent));
}

void QnResourceTreeWidget::at_resourceProxyModel_modelReset()
{
    std::function<QModelIndexList(const QModelIndex&)> getChildren;
    getChildren =
        [this, &getChildren](const QModelIndex& parent)
        {
            QModelIndexList result;
            for (int row = 0; row < m_resourceProxyModel->rowCount(parent); ++row)
            {
                const auto childIndex = m_resourceProxyModel->index(row, 0, parent);
                result.append(childIndex);
                result.append(getChildren(childIndex));
            }
            return result;
        };
    for (const auto& index: getChildren(QModelIndex()))
    {
        if (m_autoExpandPolicy && m_autoExpandPolicy(index))
            ui->resourcesTreeView->expand(index);
    }
}

void QnResourceTreeWidget::expandNodeIfNeeded(const QModelIndex& index)
{
    bool needExpand = m_autoExpandPolicy
        ? m_autoExpandPolicy(index)
        : false;

    if (needExpand)
        ui->resourcesTreeView->expand(index);

    at_resourceProxyModel_rowsInserted(index, 0, m_resourceProxyModel->rowCount(index) - 1);
}

void QnResourceTreeWidget::update(const QnResourcePtr& resource)
{
    if (!resource)
        return;

    const auto model = ui->resourcesTreeView->model();
    if (!model)
        return;

    const auto start = model->index(0, 0, ui->resourcesTreeView->rootIndex());

    const auto indices = model->match(
        start,
        Qn::ResourceRole,
        QVariant::fromValue(resource),
        -1,
        Qt::MatchExactly | Qt::MatchRecursive);

    for (const auto& index: indices)
        ui->resourcesTreeView->update(index);
}

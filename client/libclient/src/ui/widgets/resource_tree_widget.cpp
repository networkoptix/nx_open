#include "resource_tree_widget.h"
#include "ui_resource_tree_widget.h"

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QScrollBar>
#include <QtGui/QContextMenuEvent>

#include <common/common_meta_types.h>

#include <nx/utils/string.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/common/indents.h>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource/resource_compare_helper.h>

#include <ui/style/noptix_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workaround/hidpi_workarounds.h>

// -------------------------------------------------------------------------- //
// QnResourceTreeSortProxyModel
// -------------------------------------------------------------------------- //
class QnResourceTreeSortProxyModel: public QnResourceSearchProxyModel
{
    typedef QnResourceSearchProxyModel base_type;

public:
    QnResourceTreeSortProxyModel(QObject *parent = NULL):
        base_type(parent),
        m_filterEnabled(false)
    {
    }

    bool isFilterEnabled()
    {
        return m_filterEnabled;
    }

    void setFilterEnabled(bool enabled)
    {
        if (m_filterEnabled == enabled)
            return;
        m_filterEnabled = enabled;
        invalidateFilter();
    }

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        if (index.column() == Qn::CheckColumn && role == Qt::CheckStateRole)
        {
            //TODO: #vkutin #GDM #common Maybe move these signals to QnResourceTreeModel
            emit beforeRecursiveOperation();
            base_type::setData(index, value, Qt::CheckStateRole);
            emit afterRecursiveOperation();
            return true;
        }

        return base_type::setData(index, value, role);
    }

    Qt::DropActions supportedDropActions() const override
    {
        return sourceModel()->supportedDropActions();
    }

private:
    /**
     * Helper function to list nodes in the correct order.
     * Root nodes are strictly ordered, but there is one type of node which is inserted in between:
     * videowall nodes, which are pinned between Layouts and WebPages.
     */
    qreal nodeOrder(const QModelIndex &index) const
    {
        Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
        if (nodeType != Qn::ResourceNode)
            return nodeType;

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        bool isVideoWall = resource->flags().testFlag(Qn::videowall);
        if (isVideoWall)
            return 0.5 * (Qn::LayoutsNode + Qn::WebPagesNode);

        return nodeType;
    }


protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const
    {
        qreal leftOrder = nodeOrder(left);
        qreal rightOrder = nodeOrder(right);
        if (!qFuzzyEquals(leftOrder, rightOrder))
            return leftOrder < rightOrder;

        return resourceLessThan(left, right);
    }

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (!m_filterEnabled)
            return true;

        QModelIndex root = (sourceParent.column() > Qn::NameColumn)
            ? sourceParent.sibling(sourceParent.row(), Qn::NameColumn)
            : sourceParent;
        return base_type::filterAcceptsRow(sourceRow, root);
    }

private:
    bool m_filterEnabled;
};


// -------------------------------------------------------------------------- //
// QnResourceTreeWidget
// -------------------------------------------------------------------------- //
QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::QnResourceTreeWidget()),
    m_criterion(),
    m_itemDelegate(nullptr),
    m_resourceProxyModel(nullptr),
    m_checkboxesVisible(true),
    m_graphicsTweaksFlags(0),
    m_editingEnabled(false),
    m_simpleSelectionEnabled(false)
{
    ui->setupUi(this);

    //TODO: #vkutin replace with SearchLineEdit
    ui->filterLineEdit->addAction(qnSkin->icon("theme/input_search.png"),
        QLineEdit::LeadingPosition);
    ui->filterLineEdit->setClearButtonEnabled(true);
    ui->filter->setVisible(false);

    m_itemDelegate = new QnResourceItemDelegate(this);
    m_itemDelegate->setFixedHeight(0); // automatic height
    ui->resourcesTreeView->setItemDelegateForColumn(Qn::NameColumn, m_itemDelegate);
    ui->resourcesTreeView->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(0, 1)));

    connect(ui->resourcesTreeView, &QnTreeView::enterPressed, this,
        [this](const QModelIndex& index){emit activated(index, false); });
    connect(ui->resourcesTreeView, &QnTreeView::doubleClicked, this,
        [this](const QModelIndex& index){emit activated(index, true); });

    connect(ui->resourcesTreeView, &QnTreeView::spacePressed, this,
        &QnResourceTreeWidget::at_treeView_spacePressed);
    connect(ui->resourcesTreeView, &QnTreeView::clicked, this,
        &QnResourceTreeWidget::at_treeView_clicked);

    connect(ui->filterLineEdit, &QLineEdit::textChanged, this,
        &QnResourceTreeWidget::updateFilter);
    connect(ui->filterLineEdit, &QLineEdit::editingFinished, this,
        &QnResourceTreeWidget::updateFilter);

    ui->resourcesTreeView->installEventFilter(this);
}

QnResourceTreeWidget::~QnResourceTreeWidget()
{
    setWorkbench(NULL);
}

QAbstractItemModel *QnResourceTreeWidget::model() const
{
    return m_resourceProxyModel
        ? m_resourceProxyModel->sourceModel()
        : nullptr;
}

void QnResourceTreeWidget::setModel(QAbstractItemModel *model)
{
    if (m_resourceProxyModel)
    {
        disconnect(m_resourceProxyModel, NULL, this, NULL);
        delete m_resourceProxyModel;
        m_resourceProxyModel = NULL;
    }

    if (model)
    {
        m_resourceProxyModel = new QnResourceTreeSortProxyModel(this);
        m_resourceProxyModel->setSourceModel(model);
        m_resourceProxyModel->setDynamicSortFilter(true);
        m_resourceProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_resourceProxyModel->setFilterKeyColumn(Qn::NameColumn);
        m_resourceProxyModel->setFilterRole(Qn::ResourceSearchStringRole);
        m_resourceProxyModel->setSortRole(Qt::DisplayRole);
        m_resourceProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_resourceProxyModel->sort(Qn::NameColumn);
        m_resourceProxyModel->setFilterEnabled(false);

        ui->resourcesTreeView->setModel(m_resourceProxyModel);

        connect(m_resourceProxyModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(at_resourceProxyModel_rowsInserted(const QModelIndex &, int, int)));
        connect(m_resourceProxyModel, &QnResourceSearchProxyModel::beforeRecursiveOperation, this, &QnResourceTreeWidget::beforeRecursiveOperation);
        connect(m_resourceProxyModel, &QnResourceSearchProxyModel::afterRecursiveOperation, this, &QnResourceTreeWidget::afterRecursiveOperation);
        at_resourceProxyModel_rowsInserted(QModelIndex());

        updateFilter();
        updateColumns();
    }
    else
    {
        ui->resourcesTreeView->setModel(NULL);
    }
}

const QnResourceCriterion &QnResourceTreeWidget::criterion() const
{
    return m_criterion;
}

void QnResourceTreeWidget::setCriterion(const QnResourceCriterion &criterion)
{
    m_criterion = criterion;

    updateFilter();
}

QItemSelectionModel* QnResourceTreeWidget::selectionModel()
{
    return ui->resourcesTreeView->selectionModel();
}

void QnResourceTreeWidget::setWorkbench(QnWorkbench *workbench)
{
    m_itemDelegate->setWorkbench(workbench);
}

void QnResourceTreeWidget::edit()
{
    QAbstractItemView* view = ui->resourcesTreeView;
    view->edit(selectionModel()->currentIndex());
}

void QnResourceTreeWidget::expand(const QModelIndex &index)
{
    ui->resourcesTreeView->expand(index);
}

void QnResourceTreeWidget::expandAll()
{
    ui->resourcesTreeView->expandAll();
}

void QnResourceTreeWidget::expandChecked()
{
    auto model = ui->resourcesTreeView->model();

    for (int i = 0; i < model->rowCount(ui->resourcesTreeView->rootIndex()); ++i)
        expandCheckedRecursively(model->index(i, Qn::NameColumn));
}

void QnResourceTreeWidget::expandCheckedRecursively(const QModelIndex& from)
{
    if (!from.isValid())
        return;

    auto checkStateIndex = from.sibling(from.row(), Qn::CheckColumn);
    if (checkStateIndex.data(Qt::CheckStateRole).toInt() != Qt::Unchecked)
        expand(from);

    for (int i = 0; i < from.model()->rowCount(from); ++i)
        expandCheckedRecursively(from.child(i, Qn::NameColumn));
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

void QnResourceTreeWidget::setCheckboxesVisible(bool visible)
{
    if (m_checkboxesVisible == visible)
        return;
    m_checkboxesVisible = visible;
    updateColumns();
}

bool QnResourceTreeWidget::isCheckboxesVisible() const
{
    return m_checkboxesVisible;
}

QnResourceTreeModelCustomColumnDelegate* QnResourceTreeWidget::customColumnDelegate() const
{
    QAbstractItemModel* sourceModel = model();
    while (QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(sourceModel))
        sourceModel = proxy->sourceModel();

    if (QnResourceTreeModel* resourceModel = qobject_cast<QnResourceTreeModel*>(sourceModel))
        return resourceModel->customColumnDelegate();

    return nullptr;
}

void QnResourceTreeWidget::setCustomColumnDelegate(QnResourceTreeModelCustomColumnDelegate *columnDelegate)
{
    QAbstractItemModel* sourceModel = model();
    while (QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(sourceModel))
        sourceModel = proxy->sourceModel();

    QnResourceTreeModel* resourceModel = qobject_cast<QnResourceTreeModel*>(sourceModel);
    NX_ASSERT(resourceModel != nullptr, Q_FUNC_INFO, "Invalid model");

    if (!resourceModel)
        return;

    resourceModel->setCustomColumnDelegate(columnDelegate);
    updateColumns();
}

void QnResourceTreeWidget::setGraphicsTweaks(Qn::GraphicsTweaksFlags flags)
{
    if (m_graphicsTweaksFlags == flags)
        return;

    m_graphicsTweaksFlags = flags;

    /*
     * Currently this is not working: hidden row does not receive full update when scrolled up.
     * It was working only due to full tree repainting inside graphics proxy widget.

    if (flags & Qn::HideLastRow)
        ui->resourcesTreeView->setProperty(Qn::HideLastRowInTreeIfNotEnoughSpace, true);
    else
        ui->resourcesTreeView->setProperty(Qn::HideLastRowInTreeIfNotEnoughSpace, QVariant());
    */

    if (flags & Qn::BypassGraphicsProxy)
    {
        ui->resourcesTreeView->setWindowFlags(ui->resourcesTreeView->windowFlags() | Qt::BypassGraphicsProxyWidget);
        ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);
    }
    else
    {
        ui->resourcesTreeView->setWindowFlags(ui->resourcesTreeView->windowFlags() &~Qt::BypassGraphicsProxyWidget);
        ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() &~Qt::BypassGraphicsProxyWidget);
    }

}

Qn::GraphicsTweaksFlags QnResourceTreeWidget::graphicsTweaks()
{
    return m_graphicsTweaksFlags;
}

void QnResourceTreeWidget::setFilterVisible(bool visible)
{
    ui->filter->setVisible(visible);
}

bool QnResourceTreeWidget::isFilterVisible() const
{
    return ui->filter->isVisible();
}

void QnResourceTreeWidget::setEditingEnabled(bool enabled)
{
    if (m_editingEnabled == enabled)
        return;

    m_editingEnabled = enabled;

    if (m_editingEnabled)
    {
        ui->resourcesTreeView->setAcceptDrops(true);
        ui->resourcesTreeView->setDragDropMode(QAbstractItemView::DragDrop);
        ui->resourcesTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    }
    else
    {
        ui->resourcesTreeView->setAcceptDrops(false);
        ui->resourcesTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
        ui->resourcesTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

bool QnResourceTreeWidget::isEditingEnabled() const
{
    return m_editingEnabled;
}

void QnResourceTreeWidget::setSimpleSelectionEnabled(bool enabled)
{
    if (m_simpleSelectionEnabled == enabled)
        return;

    m_simpleSelectionEnabled = enabled;

    if (m_simpleSelectionEnabled)
    {
        ui->resourcesTreeView->setSelectionMode(QAbstractItemView::NoSelection);
    }
    else
    {
        ui->resourcesTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
}

bool QnResourceTreeWidget::isSimpleSelectionEnabled() const
{
    return m_simpleSelectionEnabled;
}

QAbstractItemView* QnResourceTreeWidget::treeView() const
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

    ui->resourcesTreeView->setColumnHidden(Qn::CheckColumn, !m_checkboxesVisible);
    if (m_checkboxesVisible)
        ui->resourcesTreeView->header()->setSectionResizeMode(Qn::CheckColumn, QHeaderView::ResizeToContents);

    bool customColumnVisible = (customColumnDelegate() != nullptr);

    ui->resourcesTreeView->setColumnHidden(Qn::CustomColumn, !customColumnVisible);
    if (customColumnVisible)
        ui->resourcesTreeView->header()->setSectionResizeMode(Qn::CustomColumn, QHeaderView::ResizeToContents);

    ui->resourcesTreeView->header()->setSectionResizeMode(Qn::NameColumn, QHeaderView::Stretch);
}

void QnResourceTreeWidget::updateFilter()
{
    QString filter = ui->filterLineEdit->text();

    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty())
    {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    m_resourceProxyModel->clearCriteria();
    m_resourceProxyModel->addCriterion(QnResourceCriterionGroup(filter));
    m_resourceProxyModel->addCriterion(m_criterion);
    m_resourceProxyModel->addCriterion(QnResourceCriterion(Qn::server));

    m_resourceProxyModel->setFilterEnabled(!filter.isEmpty() || !m_criterion.isNull());
    if (!filter.isEmpty())
        ui->resourcesTreeView->expandAll();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnResourceTreeWidget::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::ContextMenu:
            if (obj == ui->resourcesTreeView)
            {
                QContextMenuEvent* me = static_cast<QContextMenuEvent *>(event);
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

void QnResourceTreeWidget::mousePressEvent(QMouseEvent *event)
{
    base_type::mousePressEvent(event);
}

void QnResourceTreeWidget::at_treeView_spacePressed(const QModelIndex &index)
{
    if (!m_checkboxesVisible)
        return;

    QModelIndex checkedIdx = index.sibling(index.row(), Qn::CheckColumn);

    bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    int inverted = checked ? Qt::Unchecked : Qt::Checked;
    m_resourceProxyModel->setData(checkedIdx, inverted, Qt::CheckStateRole);
}

void QnResourceTreeWidget::at_treeView_clicked(const QModelIndex &index)
{
    if (!m_simpleSelectionEnabled)
        return;

    if (index.column() == Qn::CheckColumn)
        return; /* Will be processed by delegate. */

    QModelIndex checkIndex = index.sibling(index.row(), Qn::CheckColumn);
    if (QAbstractItemModel *model = ui->resourcesTreeView->model())
    {
        int checkState = model->data(checkIndex, Qt::CheckStateRole).toInt();
        if (checkState == Qt::Checked)
            checkState = Qt::Unchecked;
        else
            checkState = Qt::Checked;
        model->setData(checkIndex, checkState, Qt::CheckStateRole);
    }
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end)
{
    for (int i = start; i <= end; i++)
        at_resourceProxyModel_rowsInserted(m_resourceProxyModel->index(i, 0, parent));
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &index)
{
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if ((resource && resource->hasFlags(Qn::server))
        || nodeType == Qn::ServersNode
        || nodeType == Qn::UserResourcesNode)
    {
        ui->resourcesTreeView->expand(index);
    }
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

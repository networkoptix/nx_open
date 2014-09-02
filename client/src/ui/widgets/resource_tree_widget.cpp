#include "resource_tree_widget.h"
#include "ui_resource_tree_widget.h"

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QScrollBar>
#include <QtGui/QContextMenuEvent>

#include <common/common_meta_types.h>

#include <utils/common/string.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/delegates/resource_tree_item_delegate.h>
#include <ui/models/resource_search_proxy_model.h>

#include <ui/style/noptix_style.h>
#include <ui/style/proxy_style.h>
#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

// -------------------------------------------------------------------------- //
// QnResourceTreeSortProxyModel
// -------------------------------------------------------------------------- //
class QnResourceTreeSortProxyModel: public QnResourceSearchProxyModel {
    typedef QnResourceSearchProxyModel base_type;

public:
    QnResourceTreeSortProxyModel(QObject *parent = NULL):
        base_type(parent),
        m_filterEnabled(false)
    {}

    bool isFilterEnabled() { 
        return m_filterEnabled; 
    }

    void setFilterEnabled(bool enabled) {
        if (m_filterEnabled == enabled)
            return;
        m_filterEnabled = enabled;
        invalidateFilter();
    }

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        if (index.column() == Qn::CheckColumn && role == Qt::CheckStateRole) {
            Qt::CheckState checkState = (Qt::CheckState) value.toInt();
            emit beforeRecursiveOperation();
            setCheckStateRecursive(index, checkState);
            setCheckStateRecursiveUp(index, checkState);
            emit afterRecursiveOperation();
            return true;
        } else
            return base_type::setData(index, value, role);
    }

    Qt::DropActions supportedDropActions() const override {
        return sourceModel()->supportedDropActions();
    }


protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
        Qn::NodeType leftNode = left.data(Qn::NodeTypeRole).value<Qn::NodeType>();
        Qn::NodeType rightNode = right.data(Qn::NodeTypeRole).value<Qn::NodeType>();

        /* Local node must be the last one in a list. */
        bool leftLocal = leftNode == Qn::LocalNode;
        bool rightLocal = rightNode == Qn::LocalNode;
        if(leftLocal ^ rightLocal) /* One of the nodes is a local node, but not both. */
            return rightLocal;

        QnResourcePtr leftResource = left.data(Qn::ResourceRole).value<QnResourcePtr>();
        QnResourcePtr rightResource = right.data(Qn::ResourceRole).value<QnResourcePtr>();
        bool leftVideoWall = leftResource.dynamicCast<QnVideoWallResource>();
        bool rightVideoWall = rightResource.dynamicCast<QnVideoWallResource>();
        if(leftVideoWall ^ rightVideoWall) /* One of the nodes is a videowall node, but not both. */
            return rightVideoWall;

        // checking pairs of VideoWallItemNode + VideoWallMatrixNode
        if ((leftNode == Qn::VideoWallItemNode || rightNode == Qn::VideoWallItemNode)
            && leftNode != rightNode)
            return rightNode == Qn::VideoWallItemNode;   

        /* Sort by name. */
        QString leftDisplay = left.data(Qt::DisplayRole).toString();
        QString rightDisplay = right.data(Qt::DisplayRole).toString();
        int result = naturalStringCompare(leftDisplay, rightDisplay, Qt::CaseInsensitive , false );
        if(result != 0)
            return result < 0;

        /* We want the order to be defined even for items with the same name. */
        if(leftResource && rightResource) {
            return leftResource->getUniqueId() < rightResource->getUniqueId();
        } else {
            return leftResource < rightResource;
        }
    }

    void setCheckStateRecursive(const QModelIndex &index, Qt::CheckState state) {
        QModelIndex root = index.sibling(index.row(), Qn::NameColumn);
        for (int i = 0; i < rowCount(root); ++i)
            setCheckStateRecursive(this->index(i, Qn::CheckColumn, root), state);
        base_type::setData(index, state, Qt::CheckStateRole);
    }

    void setCheckStateRecursiveUp(const QModelIndex &index, Qt::CheckState state) {
        QModelIndex root = index.parent();
        if (!root.isValid())
            return;

        for (int i = 0; (i < rowCount(root)) && (state == Qt::Checked); ++i)
            if (this->index(i, Qn::CheckColumn, root).data(Qt::CheckStateRole).toInt() != Qt::Checked)
                state = Qt::Unchecked;

        QModelIndex checkRoot = root.sibling(root.row(), Qn::CheckColumn);
        if (checkRoot.data(Qt::CheckStateRole) == state)
            return;
        base_type::setData(checkRoot, state, Qt::CheckStateRole);
        setCheckStateRecursiveUp(root, state);
    }

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
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
QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnResourceTreeWidget()),
    m_resourceProxyModel(0),
    m_checkboxesVisible(true),
    m_graphicsTweaksFlags(0),
    m_editingEnabled(false),
    m_simpleSelectionEnabled(false)
{
    ui->setupUi(this);
    ui->filterFrame->setVisible(false);
    ui->selectFilterButton->setVisible(false);

    m_itemDelegate = new QnResourceTreeItemDelegate(this);
    ui->resourcesTreeView->setItemDelegate(m_itemDelegate);

    connect(ui->resourcesTreeView,      SIGNAL(enterPressed(QModelIndex)),  this,               SLOT(at_treeView_enterPressed(QModelIndex)));
    connect(ui->resourcesTreeView,      SIGNAL(spacePressed(QModelIndex)),  this,               SLOT(at_treeView_spacePressed(QModelIndex)));
    connect(ui->resourcesTreeView,      SIGNAL(doubleClicked(QModelIndex)), this,               SLOT(at_treeView_doubleClicked(QModelIndex)));
    connect(ui->resourcesTreeView,      SIGNAL(clicked(QModelIndex)),       this,               SLOT(at_treeView_clicked(QModelIndex)));

    connect(this,                       SIGNAL(viewportSizeChanged()),      this,               SLOT(updateColumnsSize()), Qt::QueuedConnection);

    connect(ui->filterLineEdit,         SIGNAL(textChanged(QString)),       this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,         SIGNAL(editingFinished()),          this,               SLOT(updateFilter()));

    connect(ui->clearFilterButton,      SIGNAL(clicked()),                  ui->filterLineEdit, SLOT(clear()));

    ui->resourcesTreeView->installEventFilter(this);
    ui->resourcesTreeView->verticalScrollBar()->installEventFilter(this);
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    setWorkbench(NULL);
}

QAbstractItemModel *QnResourceTreeWidget::model() const {
    return m_resourceProxyModel ? m_resourceProxyModel->sourceModel() : NULL;
}

void QnResourceTreeWidget::setModel(QAbstractItemModel *model) {
    if (m_resourceProxyModel) {
        disconnect(m_resourceProxyModel, NULL, this, NULL);
        delete m_resourceProxyModel;
        m_resourceProxyModel = NULL;
    }

    if (model) {
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

        connect(m_resourceProxyModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),   this, SLOT(at_resourceProxyModel_rowsInserted(const QModelIndex &, int, int)));
        connect(m_resourceProxyModel, &QnResourceSearchProxyModel::beforeRecursiveOperation, this, &QnResourceTreeWidget::beforeRecursiveOperation);
        connect(m_resourceProxyModel, &QnResourceSearchProxyModel::afterRecursiveOperation, this, &QnResourceTreeWidget::afterRecursiveOperation);
        at_resourceProxyModel_rowsInserted(QModelIndex());

        updateCheckboxesVisibility();
        updateFilter();
    } else {
        ui->resourcesTreeView->setModel(NULL);
    }

    emit viewportSizeChanged();
}

const QnResourceCriterion &QnResourceTreeWidget::criterion() const {
    return m_criterion;
}

void QnResourceTreeWidget::setCriterion(const QnResourceCriterion &criterion) {
    m_criterion = criterion;
    
    updateFilter();
}

QItemSelectionModel* QnResourceTreeWidget::selectionModel() {
    return ui->resourcesTreeView->selectionModel();
}

void QnResourceTreeWidget::setWorkbench(QnWorkbench *workbench) {
    m_itemDelegate->setWorkbench(workbench);
}

void QnResourceTreeWidget::edit() {
    ui->resourcesTreeView->edit(selectionModel()->currentIndex());
}

void QnResourceTreeWidget::expand(const QModelIndex &index) {
    ui->resourcesTreeView->expand(index);
}

void QnResourceTreeWidget::expandAll() {
    ui->resourcesTreeView->expandAll();
}

QPoint QnResourceTreeWidget::selectionPos() const {
    QModelIndexList selectedRows = ui->resourcesTreeView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return QPoint();

    QModelIndex selected = selectedRows.back();
    QPoint pos = ui->resourcesTreeView->visualRect(selected).bottomRight();
    pos = ui->resourcesTreeView->mapToGlobal(pos);
    return pos;
}

void QnResourceTreeWidget::setCheckboxesVisible(bool visible) {
    if (m_checkboxesVisible == visible)
        return;
    m_checkboxesVisible = visible;
    updateCheckboxesVisibility();
}

bool QnResourceTreeWidget::isCheckboxesVisible() const {
    return m_checkboxesVisible;
}

void QnResourceTreeWidget::setGraphicsTweaks(Qn::GraphicsTweaksFlags flags) {
    if (m_graphicsTweaksFlags == flags)
        return;

    m_graphicsTweaksFlags = flags;

    if (flags & Qn::HideLastRow)
        ui->resourcesTreeView->setProperty(Qn::HideLastRowInTreeIfNotEnoughSpace, true);
    else
        ui->resourcesTreeView->setProperty(Qn::HideLastRowInTreeIfNotEnoughSpace, QVariant());

    if (flags & Qn::BackgroundOpacity)
        ui->resourcesTreeView->setProperty(Qn::ItemViewItemBackgroundOpacity, 0.5);
    else
        ui->resourcesTreeView->setProperty(Qn::ItemViewItemBackgroundOpacity, QVariant());

    if (flags & Qn::BypassGraphicsProxy) {
        ui->resourcesTreeView->setWindowFlags(ui->resourcesTreeView->windowFlags() | Qt::BypassGraphicsProxyWidget);
        ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);
    } else {
        ui->resourcesTreeView->setWindowFlags(ui->resourcesTreeView->windowFlags() &~ Qt::BypassGraphicsProxyWidget);
        ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() &~ Qt::BypassGraphicsProxyWidget);
    }

}

void QnResourceTreeWidget::setFilterVisible(bool visible) {
    ui->filterFrame->setVisible(visible);
}

bool QnResourceTreeWidget::isFilterVisible() const {
    return ui->filterFrame->isVisible();
}

void QnResourceTreeWidget::setEditingEnabled(bool enabled) {
    if (m_editingEnabled == enabled)
        return;

    m_editingEnabled = enabled;

    if (m_editingEnabled) {
        ui->resourcesTreeView->setAcceptDrops(true);
        ui->resourcesTreeView->setDragDropMode(QAbstractItemView::DragDrop);
        ui->resourcesTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    } else {
        ui->resourcesTreeView->setAcceptDrops(false);
        ui->resourcesTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
        ui->resourcesTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

bool QnResourceTreeWidget::isEditingEnabled() const {
    return m_editingEnabled;
}

void QnResourceTreeWidget::setSimpleSelectionEnabled(bool enabled) {
    if(m_simpleSelectionEnabled == enabled)
        return;

    m_simpleSelectionEnabled = enabled;

    if(m_simpleSelectionEnabled) {
        ui->resourcesTreeView->setSelectionMode(QAbstractItemView::NoSelection);
    } else {
        ui->resourcesTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
}

bool QnResourceTreeWidget::isSimpleSelectionEnabled() const {
    return m_simpleSelectionEnabled;
}

QAbstractItemView* QnResourceTreeWidget::treeView() const {
    return ui->resourcesTreeView;
}

void QnResourceTreeWidget::updateCheckboxesVisibility() {
    ui->resourcesTreeView->setColumnHidden(1, !m_checkboxesVisible);
}

void QnResourceTreeWidget::updateColumnsSize() {
    const int checkBoxSize = 16;
    const int offset = checkBoxSize * 1.5;
    ui->resourcesTreeView->setColumnWidth(1, checkBoxSize);
    ui->resourcesTreeView->setColumnWidth(0, ui->resourcesTreeView->viewport()->width() - offset);
}

void QnResourceTreeWidget::updateFilter() {
    QString filter = ui->filterLineEdit->text();

    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    ui->clearFilterButton->setVisible(!filter.isEmpty());

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
bool QnResourceTreeWidget::eventFilter(QObject *obj, QEvent *event){
    if (obj == ui->resourcesTreeView->verticalScrollBar() &&
        (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
            emit viewportSizeChanged();
    } else if (obj == ui->resourcesTreeView && event->type() == QEvent::ContextMenu) {
        QContextMenuEvent* me = static_cast<QContextMenuEvent *>(event);
        if (me->reason() == QContextMenuEvent::Mouse
                && !ui->resourcesTreeView->indexAt(me->pos()).isValid())
            selectionModel()->clear();
    }
    return base_type::eventFilter(obj, event);
}

void QnResourceTreeWidget::resizeEvent(QResizeEvent *event) {
    emit viewportSizeChanged();
    base_type::resizeEvent(event);
}

void QnResourceTreeWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);
}

void QnResourceTreeWidget::at_treeView_enterPressed(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        emit activated(resource);
}

void QnResourceTreeWidget::at_treeView_spacePressed(const QModelIndex &index) {
    if (!m_checkboxesVisible)
        return;
    QModelIndex checkedIdx = index.sibling(index.row(), Qn::CheckColumn);

    bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
    int inverted = checked ? Qt::Unchecked : Qt::Checked;
    m_resourceProxyModel->setData(checkedIdx, inverted, Qt::CheckStateRole);
}


void QnResourceTreeWidget::at_treeView_doubleClicked(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

    // TODO: #Elric. This is totally evil. This check belongs to the slot that handles activation.
    if (resource &&
        !(resource->flags() & Qn::layout) &&    /* Layouts cannot be activated by double clicking. */
        !(resource->flags() & Qn::server))      /* Bug #1009: Servers should not be activated by double clicking. */
        emit activated(resource);
}

void QnResourceTreeWidget::at_treeView_clicked(const QModelIndex &index) {
    if(!m_simpleSelectionEnabled)
        return;

    if(index.column() == Qn::CheckColumn)
        return; /* Will be processed by delegate. */

    QModelIndex checkIndex = index.sibling(index.row(), Qn::CheckColumn);
    if(QAbstractItemModel *model = ui->resourcesTreeView->model()) {
        int checkState = model->data(checkIndex, Qt::CheckStateRole).toInt();
        if(checkState == Qt::Checked) {
            checkState = Qt::Unchecked;
        } else {
            checkState = Qt::Checked;
        }
        model->setData(checkIndex, checkState, Qt::CheckStateRole);
    }
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end) {
    for(int i = start; i <= end; i++)
        at_resourceProxyModel_rowsInserted(m_resourceProxyModel->index(i, 0, parent));
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if((resource && resource->hasFlags(Qn::server)) || nodeType == Qn::ServersNode)
        ui->resourcesTreeView->expand(index);
    at_resourceProxyModel_rowsInserted(index, 0, m_resourceProxyModel->rowCount(index) - 1);
}

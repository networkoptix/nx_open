#include "resource_tree_widget.h"
#include "ui_resource_tree_widget.h"

#include <QtGui/QStyledItemDelegate>

#include <common/common_meta_types.h>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/style/noptix_style.h>
#include <ui/style/proxy_style.h>
#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

// ------ Delegate class -----

class QnResourceTreeItemDelegate: public QStyledItemDelegate {
    typedef QStyledItemDelegate base_type;

public:
    explicit QnResourceTreeItemDelegate(QObject *parent = NULL):
        base_type(parent)
    {
        m_recordingIcon = qnSkin->icon("tree/recording.png");
        m_raisedIcon = qnSkin->icon("tree/raised.png");
        m_scheduledIcon = qnSkin->icon("tree/scheduled.png");
    }

    QnWorkbench *workbench() const {
        return m_workbench.data();
    }

    void setWorkbench(QnWorkbench *workbench) {
        m_workbench = workbench;
    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        if (index.column() == 1){
            base_type::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItemV4 optionV4 = option;
        initStyleOption(&optionV4, index);

        if(optionV4.widget && optionV4.widget->rect().bottom() < optionV4.rect.bottom()
                && optionV4.widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
            return;

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        QnResourcePtr currentLayoutResource = workbench() ? workbench()->currentLayout()->resource() : QnLayoutResourcePtr();
        QnResourcePtr parentResource = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
        QUuid uuid = index.data(Qn::ItemUuidRole).value<QUuid>();

        /* Bold items of current layout in tree. */
        if(!resource.isNull() && !currentLayoutResource.isNull()) {
            bool bold = false;
            if(resource == currentLayoutResource) {
                bold = true; /* Bold current layout. */
            } else if(parentResource == currentLayoutResource) {
                bold = true; /* Bold items of the current layout. */
            } else if(uuid.isNull() && workbench() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()) {
                bold = true; /* Bold items of the current layout in servers. */
            }

            optionV4.font.setBold(bold);
        }

        QStyle *style = optionV4.widget ? optionV4.widget->style() : QApplication::style();

        /* Highlight currently raised/zoomed item. */
        QnWorkbenchItem *raisedItem = NULL;
        if(workbench()) {
            raisedItem = workbench()->item(Qn::RaisedRole);
            if(!raisedItem)
                raisedItem = workbench()->item(Qn::ZoomedRole);
        }

        QRect decorationRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &optionV4, optionV4.widget);

        if(raisedItem && (raisedItem->uuid() == uuid || (resource && uuid.isNull() && raisedItem->resourceUid() == resource->getUniqueId()))) {
            m_raisedIcon.paint(painter, decorationRect);

            QRect rect = optionV4.rect;
            QRect skipRect(
                rect.topLeft(),
                QPoint(
                    decorationRect.right() + decorationRect.left() - rect.left(),
                    rect.bottom()
                )
            );
            rect.setLeft(skipRect.right() + 1);

            optionV4.rect = skipRect;
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &optionV4, painter, optionV4.widget);
            optionV4.rect = rect;
        }

        /* Draw 'recording' icon. */
        bool recording = false, scheduled = false;
        if(resource) {
            if(resource->getStatus() == QnResource::Recording) {
                recording = true;
            } else if(QnNetworkResourcePtr camera = resource.dynamicCast<QnNetworkResource>()) {
                foreach(const QnNetworkResourcePtr &otherCamera, QnCameraHistoryPool::instance()->getAllCamerasWithSamePhysicalId(camera)) {
                    if(otherCamera->getStatus() == QnResource::Recording) {
                        recording = true;
                        break;
                    }
                }
            }

            if(!recording)
                if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
                    scheduled = !camera->isScheduleDisabled();
        }

        if(recording || scheduled) {
            QRect iconRect = decorationRect;
            iconRect.moveLeft(iconRect.left() - iconRect.width() - 2);

            (recording ? m_recordingIcon : m_scheduledIcon).paint(painter, iconRect);
        }

        /* Draw item. */
        style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter, optionV4.widget);
    }

    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        base_type::initStyleOption(option, index);

    }

private:
    QWeakPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon, m_scheduledIcon, m_raisedIcon;
};

// ------ Style class -----

class QnResourceTreeStyle: public QnProxyStyle {
public:
    explicit QnResourceTreeStyle(QStyle *baseStyle, QObject *parent = NULL): QnProxyStyle(baseStyle, parent) {}

    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override {
        switch(element) {
        case PE_PanelItemViewItem:
        case PE_PanelItemViewRow:
            /* Don't draw elements that are only partially visible.
             * Note that this won't work with partial updates of tree widget's viewport. */
            if(widget && widget->rect().bottom() < option->rect.bottom()
                    && widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
                return;
            break;
        default:
            break;
        }

        QnProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

// ------ Sort model class ------

class QnResourceTreeSortProxyModel: public QSortFilterProxyModel {
public:
    QnResourceTreeSortProxyModel(QObject *parent = NULL): QSortFilterProxyModel(parent) {}

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
        bool leftLocal = left.data(Qn::NodeTypeRole).toInt() == Qn::LocalNode;
        bool rightLocal = right.data(Qn::NodeTypeRole).toInt() == Qn::LocalNode;
        if(leftLocal ^ rightLocal) /* One of the nodes is a local node, but not both. */
            return rightLocal;

        QString leftDisplay = left.data(Qt::DisplayRole).toString();
        QString rightDisplay = right.data(Qt::DisplayRole).toString();
        int result = leftDisplay.compare(rightDisplay);
        if(result < 0) {
            return true;
        } else if(result > 0) {
            return false;
        }

        /* We want the order to be defined even for items with the same name. */
        QnResourcePtr leftResource = left.data(Qn::ResourceRole).value<QnResourcePtr>();
        QnResourcePtr rightResource = right.data(Qn::ResourceRole).value<QnResourcePtr>();
        return leftResource < rightResource;
    }
};


// ------ Widget class -----

QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnResourceTreeWidget()),
    m_resourceProxyModel(0)
{
    ui->setupUi(this);
    ui->filterFrame->setVisible(false); //TODO: set visible from property

    m_itemDelegate = new QnResourceTreeItemDelegate(this);
    ui->resourcesTreeView->setItemDelegate(m_itemDelegate);
    ui->resourcesTreeView->setProperty(Qn::HideLastRowInTreeIfNotEnoughSpace, true);
    ui->resourcesTreeView->setProperty(Qn::ItemViewItemBackgroundOpacity, 0.5);

    QnResourceTreeStyle *treeStyle = new QnResourceTreeStyle(style(), this);
    ui->resourcesTreeView->setStyle(treeStyle);

    ui->resourcesTreeView->setWindowFlags(ui->resourcesTreeView->windowFlags() | Qt::BypassGraphicsProxyWidget);
    ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);

    connect(ui->resourcesTreeView,     SIGNAL(enterPressed(QModelIndex)),  this,               SLOT(at_treeView_enterPressed(QModelIndex)));
    connect(ui->resourcesTreeView,     SIGNAL(doubleClicked(QModelIndex)), this,               SLOT(at_treeView_doubleClicked(QModelIndex)));
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    setWorkbench(NULL);
}


void QnResourceTreeWidget::setModel(QAbstractItemModel *model) {
    if (m_resourceProxyModel){
        disconnect(m_resourceProxyModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(at_resourceProxyModel_rowsInserted(const QModelIndex &, int, int)));
        delete m_resourceProxyModel;
        m_resourceProxyModel = NULL;
    }

    if (model){
        m_resourceProxyModel = new QnResourceTreeSortProxyModel(this);
        m_resourceProxyModel->setSourceModel(model);
        m_resourceProxyModel->setSupportedDragActions(model->supportedDragActions());
        m_resourceProxyModel->setDynamicSortFilter(true);
        m_resourceProxyModel->setSortRole(Qt::DisplayRole);
        m_resourceProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_resourceProxyModel->sort(0);

        ui->resourcesTreeView->setModel(m_resourceProxyModel);

        connect(m_resourceProxyModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(at_resourceProxyModel_rowsInserted(const QModelIndex &, int, int)));

        at_resourceProxyModel_rowsInserted(QModelIndex());
    } else {
        ui->resourcesTreeView->setModel(NULL);
    }
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

    // mapToGlobal works incorrectly here, using two-step transformation
    pos = ui->resourcesTreeView->mapToGlobal(pos);
    return pos;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeWidget::at_treeView_enterPressed(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        emit activated(resource);
}

void QnResourceTreeWidget::at_treeView_doubleClicked(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

    if (resource &&
        !(resource->flags() & QnResource::layout) &&    /* Layouts cannot be activated by double clicking. */
        !(resource->flags() & QnResource::server))      /* Bug #1009: Servers should not be activated by double clicking. */
        emit activated(resource);
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end) {
    for(int i = start; i <= end; i++)
        at_resourceProxyModel_rowsInserted(m_resourceProxyModel->index(i, 0, parent));
}

void QnResourceTreeWidget::at_resourceProxyModel_rowsInserted(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    int nodeType = index.data(Qn::NodeTypeRole).toInt();
    if((resource && resource->hasFlags(QnResource::server)) || nodeType == Qn::ServersNode)
        ui->resourcesTreeView->expand(index);

    at_resourceProxyModel_rowsInserted(index, 0, m_resourceProxyModel->rowCount(index) - 1);
}

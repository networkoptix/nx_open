﻿#include "resource_tree_widget.h"

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QLineEdit>
#include <QtGui/QMenu>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>
#include <QtGui/QWheelEvent>
#include <QtGui/QStyledItemDelegate>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/settings.h>
#include <common/common_meta_types.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_history.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/models/resource_pool_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>

#include "ui_resource_tree_widget.h"
#include "ui/style/proxy_style.h"
#include "ui/style/noptix_style.h"
#include "ui/style/globals.h"

namespace {
    const char *qn_searchModelPropertyName = "_qn_searchModel";
    const char *qn_searchSynchronizerPropertyName = "_qn_searchSynchronizer";
    const char *qn_filterPropertyName = "_qn_filter";
    const char *qn_searchCriterionPropertyName = "_qn_searchCriterion";
}

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
        QStyleOptionViewItemV4 optionV4 = option;
        initStyleOption(&optionV4, index);

        if(optionV4.widget && optionV4.widget->rect().bottom() < optionV4.rect.bottom())
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
            } else if(uuid.isNull() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()) {
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

class QnResourceTreeStyle: public QnProxyStyle {
public:
    explicit QnResourceTreeStyle(QStyle *baseStyle, QObject *parent = NULL): QnProxyStyle(baseStyle, parent) {}

    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override {
        switch(element) {
        case PE_PanelItemViewItem:
        case PE_PanelItemViewRow:
            /* Don't draw elements that are only partially visible.
             * Note that this won't work with partial updates of tree widget's viewport. */
            if(widget && widget->rect().bottom() < option->rect.bottom())
                return;
            break;
        default:
            break;
        }

        QnProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

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


QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent, QnWorkbenchContext *context): 
    QWidget(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::ResourceTreeWidget()),
    m_ignoreFilterChanges(false),
    m_filterTimerId(0)
{
    ui->setupUi(this);

    ui->typeComboBox->addItem(tr("Any Type"), 0);
    ui->typeComboBox->addItem(tr("Video Files"), static_cast<int>(QnResource::local | QnResource::video));
    ui->typeComboBox->addItem(tr("Image Files"), static_cast<int>(QnResource::still_image));
    ui->typeComboBox->addItem(tr("Live Cameras"), static_cast<int>(QnResource::live));

    ui->clearFilterButton->setIcon(qnSkin->icon("tree/clear.png"));
    ui->clearFilterButton->setIconSize(QSize(16, 16));

    m_resourceModel = new QnResourcePoolModel(this);
    m_resourceProxyModel = new QnResourceTreeSortProxyModel(this);
    m_resourceProxyModel->setSourceModel(m_resourceModel);
    m_resourceProxyModel->setSupportedDragActions(m_resourceModel->supportedDragActions());
    m_resourceProxyModel->setDynamicSortFilter(true);
    m_resourceProxyModel->setSortRole(Qt::DisplayRole);
    m_resourceProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_resourceProxyModel->sort(0);
    ui->resourceTreeView->setModel(m_resourceProxyModel);

    m_resourceDelegate = new QnResourceTreeItemDelegate(this);
    ui->resourceTreeView->setItemDelegate(m_resourceDelegate);

    m_searchDelegate = new QnResourceTreeItemDelegate(this);
    ui->searchTreeView->setItemDelegate(m_searchDelegate);

    QnResourceTreeStyle *treeStyle = new QnResourceTreeStyle(style(), this);
    ui->resourceTreeView->setStyle(treeStyle);
    ui->searchTreeView->setStyle(treeStyle);

    ui->resourceTreeView->setProperty(Qn::ItemViewItemBackgroundOpacity, 0.5);
    ui->searchTreeView->setProperty(Qn::ItemViewItemBackgroundOpacity, 0.5);

    /* This is needed so that control's context menu is not embedded into the scene. */
    ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);
    ui->resourceTreeView->setWindowFlags(ui->resourceTreeView->windowFlags() | Qt::BypassGraphicsProxyWidget);
    ui->searchTreeView->setWindowFlags(ui->resourceTreeView->windowFlags() | Qt::BypassGraphicsProxyWidget);

    m_renameAction = new QAction(this);

    connect(ui->typeComboBox,       SIGNAL(currentIndexChanged(int)),   this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(textChanged(QString)),       this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(editingFinished()),          this,               SLOT(forceUpdateFilter()));
    connect(ui->clearFilterButton,  SIGNAL(clicked()),                  ui->filterLineEdit, SLOT(clear()));
    connect(ui->resourceTreeView,   SIGNAL(enterPressed(QModelIndex)),  this,               SLOT(at_treeView_enterPressed(QModelIndex)));
    connect(ui->resourceTreeView,   SIGNAL(doubleClicked(QModelIndex)), this,               SLOT(at_treeView_doubleClicked(QModelIndex)));
    connect(ui->searchTreeView,     SIGNAL(enterPressed(QModelIndex)),  this,               SLOT(at_treeView_enterPressed(QModelIndex)));
    connect(ui->searchTreeView,     SIGNAL(doubleClicked(QModelIndex)), this,               SLOT(at_treeView_doubleClicked(QModelIndex)));
    connect(ui->tabWidget,          SIGNAL(currentChanged(int)),        this,               SLOT(at_tabWidget_currentChanged(int)));
    connect(ui->resourceTreeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SIGNAL(selectionChanged()));
    connect(m_resourceProxyModel,   SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(at_resourceProxyModel_rowsInserted(const QModelIndex &, int, int)));

    /* Connect to context. */
    m_searchDelegate->setWorkbench(workbench());
    m_resourceDelegate->setWorkbench(workbench());

    connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),            this,   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this,   SLOT(at_workbench_currentLayoutChanged()));
    connect(workbench(),        SIGNAL(itemChanged(Qn::ItemRole)),                  this,   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(qnSettings->notifier(QnSettings::IP_SHOWN_IN_TREE), SIGNAL(valueChanged(int)), this, SLOT(at_showUrlsInTree_changed()));

    /* Run handlers. */
    updateFilter();

    at_showUrlsInTree_changed();
    at_workbench_currentLayoutChanged();
    at_resourceProxyModel_rowsInserted(QModelIndex());
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    disconnect(workbench(), NULL, this, NULL);

    at_workbench_currentLayoutAboutToBeChanged();

    m_searchDelegate->setWorkbench(NULL);
    m_resourceDelegate->setWorkbench(NULL);
}

QPalette QnResourceTreeWidget::comboBoxPalette() const {
    return ui->typeComboBox->palette();
}

void QnResourceTreeWidget::setComboBoxPalette(const QPalette &palette) {
    ui->typeComboBox->setPalette(palette);
}

QnResourceTreeWidget::Tab QnResourceTreeWidget::currentTab() const {
    return static_cast<Tab>(ui->tabWidget->currentIndex());
}

void QnResourceTreeWidget::setCurrentTab(Tab tab) {
    if(tab < 0 || tab >= TabCount) {
        qnWarning("Invalid resource tree widget tab '%1'.", static_cast<int>(tab));
        return;
    }

    ui->tabWidget->setCurrentIndex(tab);
}

bool QnResourceTreeWidget::isLayoutSearchable(QnWorkbenchLayout *layout) const {
    return accessController()->permissions(layout->resource()) & Qn::WritePermission;
}

QnResourceSearchProxyModel *QnResourceTreeWidget::layoutModel(QnWorkbenchLayout *layout, bool create) const {
    QnResourceSearchProxyModel *result = layout->property(qn_searchModelPropertyName).value<QnResourceSearchProxyModel *>();
    if(create && result == NULL && isLayoutSearchable(layout)) {
        result = new QnResourceSearchProxyModel(layout);
        result->setFilterCaseSensitivity(Qt::CaseInsensitive);
        result->setFilterKeyColumn(0);
        result->setFilterRole(Qn::ResourceSearchStringRole);
        result->setSortCaseSensitivity(Qt::CaseInsensitive);
        result->setDynamicSortFilter(true);
        result->setSourceModel(m_resourceModel);
        layout->setProperty(qn_searchModelPropertyName, QVariant::fromValue<QnResourceSearchProxyModel *>(result));

        /* Always accept servers. */
        result->addCriterion(QnResourceCriterion(QnResource::server)); 
    }
    return result;
}

QnResourceSearchSynchronizer *QnResourceTreeWidget::layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const {
    QnResourceSearchSynchronizer *result = layout->property(qn_searchSynchronizerPropertyName).value<QnResourceSearchSynchronizer *>();
    if(create && result == NULL && isLayoutSearchable(layout)) {
        result = new QnResourceSearchSynchronizer(layout);
        result->setLayout(layout);
        result->setModel(layoutModel(layout, true));
        layout->setProperty(qn_searchSynchronizerPropertyName, QVariant::fromValue<QnResourceSearchSynchronizer *>(result));
    }
    return result;
}

QString QnResourceTreeWidget::layoutFilter(QnWorkbenchLayout *layout) const {
    return layout->property(qn_filterPropertyName).toString();
}

void QnResourceTreeWidget::setLayoutFilter(QnWorkbenchLayout *layout, const QString &filter) const {
    layout->setProperty(qn_filterPropertyName, filter);
}

void QnResourceTreeWidget::killSearchTimer() {
    if (m_filterTimerId == 0) 
        return;

    killTimer(m_filterTimerId);
    m_filterTimerId = 0; 
}

void QnResourceTreeWidget::showContextMenuAt(const QPoint &pos, bool ignoreSelection) {
    if(!context() || !context()->menu()) {
        qnWarning("Requesting context menu for a tree widget while no menu manager instance is available.");
        return;
    }
    QnActionManager *manager = context()->menu();

    QScopedPointer<QMenu> menu(manager->newMenu(Qn::TreeScope, ignoreSelection ? QnActionParameters() : QnActionParameters(currentTarget(Qn::TreeScope))));

    /* Add tree-local actions to the menu. */
    manager->redirectAction(menu.data(), Qn::RenameAction, m_renameAction);
    if(currentSelectionModel()->currentIndex().data(Qn::NodeTypeRole) != Qn::UsersNode || !currentSelectionModel()->selection().contains(currentSelectionModel()->currentIndex()) || ignoreSelection)
        manager->redirectAction(menu.data(), Qn::NewUserAction, NULL); /* Show 'New User' item only when clicking on 'Users' node. */ // TODO: implement with action parameters

    if(menu->isEmpty())
        return;

    /* Run menu. */
    QAction *action = menu->exec(pos);

    /* Process tree-local actions. */
    if(action == m_renameAction)
        currentItemView()->edit(currentSelectionModel()->currentIndex());
}

QTreeView *QnResourceTreeWidget::currentItemView() const {
    if (ui->tabWidget->currentIndex() == ResourcesTab) {
        return ui->resourceTreeView;
    } else {
        return ui->searchTreeView;
    }
}

QItemSelectionModel *QnResourceTreeWidget::currentSelectionModel() const {
    return currentItemView()->selectionModel();
}

QnResourceList QnResourceTreeWidget::selectedResources() const {
    QnResourceList result;

    foreach (const QModelIndex &index, currentSelectionModel()->selectedRows()) {
        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource)
            result.append(resource);
    }

    return result;
}

QnLayoutItemIndexList QnResourceTreeWidget::selectedLayoutItems() const {
    QnLayoutItemIndexList result;

    foreach (const QModelIndex &index, currentSelectionModel()->selectedRows()) {
        QUuid uuid = index.data(Qn::ItemUuidRole).value<QUuid>();
        if(uuid.isNull())
            continue;

        QnLayoutResourcePtr layout = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnLayoutResource>();
        if(!layout)
            continue;

        result.push_back(QnLayoutItemIndex(layout, uuid));
    }

    return result;
}

Qn::ActionScope QnResourceTreeWidget::currentScope() const {
    return Qn::TreeScope;
}

QVariant QnResourceTreeWidget::currentTarget(Qn::ActionScope scope) const {
    if(scope != Qn::TreeScope)
        return QVariant();

    QItemSelectionModel *selectionModel = currentSelectionModel();

    if(!selectionModel->currentIndex().data(Qn::ItemUuidRole).value<QUuid>().isNull()) { /* If it's a layout item. */
        return QVariant::fromValue(selectedLayoutItems());
    } else {
        return QVariant::fromValue(selectedResources());
    }
}

void QnResourceTreeWidget::updateFilter(bool force) {
    QString filter = ui->filterLineEdit->text();

    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    ui->clearFilterButton->setVisible(!filter.isEmpty());
    killSearchTimer();

    if(!workbench())
        return;

    if(m_ignoreFilterChanges)
        return;

    if(!force) {
        int pos = qMax(filter.lastIndexOf(QLatin1Char('+')), filter.lastIndexOf(QLatin1Char('\\'))) + 1;
        
        /* Estimate size of the last term in filter expression. */
        int size = 0;
        for(;pos < filter.size(); pos++)
            if(!filter[pos].isSpace())
                size++;

        if (size > 0 && size < 3) 
            return; /* Filter too short, ignore. */
    }

    m_filterTimerId = startTimer(filter.isEmpty() ? 0 : 300);
}

void QnResourceTreeWidget::expandAll() {
    currentItemView()->expandAll();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeWidget::contextMenuEvent(QContextMenuEvent *event) {
    QWidget *child = childAt(event->pos());
    while(child && child != ui->resourceTreeView && child != ui->searchTreeView)
        child = child->parentWidget();

    /** 
     * Note that we cannot use event->globalPos() here as it doesn't work when
     * the widget is embedded into graphics scene.
     */
    showContextMenuAt(QCursor::pos(), !child);
}

void QnResourceTreeWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the tree widget. */
}

void QnResourceTreeWidget::mousePressEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceTreeWidget::mouseReleaseEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceTreeWidget::keyPressEvent(QKeyEvent *event) {
    event->accept();
    if (event->key() == Qt::Key_Menu) {
        if (ui->filterLineEdit->hasFocus())
            return;

        QnTreeView* treeView = ui->searchTab->isActiveWindow() 
            ? ui->searchTreeView 
            : ui->resourceTreeView; 

        QModelIndexList selectedRows = treeView->selectionModel()->selectedRows();
        if (selectedRows.isEmpty())
            return;
        
        QModelIndex selected = selectedRows.back();
        QPoint pos = treeView->visualRect(selected).bottomRight();
      
        // mapToGlobal works incorrectly here, using two-step transformation
        pos = treeView->mapToGlobal(pos);
        showContextMenuAt(display()->view()->mapToGlobal(pos));
    }
}

void QnResourceTreeWidget::keyReleaseEvent(QKeyEvent *event) {
    event->accept();
}

void QnResourceTreeWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_filterTimerId) {
        killTimer(m_filterTimerId);
        m_filterTimerId = 0;

        if (workbench()) {
            QnWorkbenchLayout *layout = workbench()->currentLayout();
            if(!isLayoutSearchable(layout)) {
                QString filter = ui->filterLineEdit->text();
                menu()->trigger(Qn::OpenNewTabAction);
                setLayoutFilter(layout, QString()); /* Clear old layout's filter. */

                layout = workbench()->currentLayout();
                ui->filterLineEdit->setText(filter);
            }

            QnResourceSearchProxyModel *model = layoutModel(layout, true);
            
            QString filter = ui->filterLineEdit->text();
            QnResource::Flags flags = static_cast<QnResource::Flags>(ui->typeComboBox->itemData(ui->typeComboBox->currentIndex()).toInt());

            model->clearCriteria();
            model->addCriterion(QnResourceCriterionGroup(filter));
            if(flags != 0)
                model->addCriterion(QnResourceCriterion(flags, QnResourceProperty::flags, QnResourceCriterion::Next, QnResourceCriterion::Reject));
            model->addCriterion(QnResourceCriterion(QnResource::server));
        }
    }

    QWidget::timerEvent(event);
}

void QnResourceTreeWidget::at_workbench_currentLayoutAboutToBeChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    disconnect(layout, NULL, this, NULL);

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->disableUpdates();
    setLayoutFilter(layout, ui->filterLineEdit->text());

    QnScopedValueRollback<bool> guard(&m_ignoreFilterChanges, true);
    ui->searchTreeView->setModel(NULL);
    ui->filterLineEdit->setText(QString());
    killSearchTimer();
}

void QnResourceTreeWidget::at_workbench_currentLayoutChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->enableUpdates();

    at_tabWidget_currentChanged(ui->tabWidget->currentIndex());

    QnScopedValueRollback<bool> guard(&m_ignoreFilterChanges, true);
    ui->filterLineEdit->setText(layoutFilter(layout));

    /* Bold state has changed. */
    currentItemView()->update();

    connect(layout,             SIGNAL(itemAdded(QnWorkbenchItem *)),               this,   SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(layout,             SIGNAL(itemRemoved(QnWorkbenchItem *)),             this,   SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
}

void QnResourceTreeWidget::at_workbench_itemChanged(Qn::ItemRole /*role*/) {
    /* Raised state has changed. */
    ui->resourceTreeView->update();
}

void QnResourceTreeWidget::at_layout_itemAdded(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceTreeWidget::at_layout_itemRemoved(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceTreeWidget::at_tabWidget_currentChanged(int index) {
    if(index == SearchTab) {
        QnWorkbenchLayout *layout = workbench()->currentLayout();

        layoutSynchronizer(layout, true); /* Just initialize the synchronizer. */
        QnResourceSearchProxyModel *model = layoutModel(layout, true);

        ui->searchTreeView->setModel(model);
        ui->searchTreeView->expandAll();

        /* View re-creates selection model for each model that is supplied to it, 
         * so we have to re-connect each time the model changes. */
        connect(ui->searchTreeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SIGNAL(selectionChanged()), Qt::UniqueConnection);
    }

    emit currentTabChanged();
}

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
        ui->resourceTreeView->expand(index);

    at_resourceProxyModel_rowsInserted(index, 0, m_resourceProxyModel->rowCount(index) - 1);
}

void QnResourceTreeWidget::at_showUrlsInTree_changed() {
    bool urlsShown = qnSettings->isIpShownInTree();

    m_resourceModel->setUrlsShown(urlsShown);
}


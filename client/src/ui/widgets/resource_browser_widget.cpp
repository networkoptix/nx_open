﻿#include "resource_browser_widget.h"
#include "ui_resource_browser_widget.h"

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

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/settings.h>

#include <common/common_meta_types.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>


#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/widgets/resource_tree_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/style/skin.h>

#include <ui/style/globals.h>

namespace {
    const char *qn_searchModelPropertyName = "_qn_searchModel";
    const char *qn_searchSynchronizerPropertyName = "_qn_searchSynchronizer";
    const char *qn_filterPropertyName = "_qn_filter";
//    const char *qn_searchCriterionPropertyName = "_qn_searchCriterion";
}

QnResourceBrowserWidget::QnResourceBrowserWidget(QWidget *parent, QnWorkbenchContext *context): 
    QWidget(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::ResourceBrowserWidget()),
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
    ui->resourceTreeWidget->setModel(m_resourceModel);
    ui->resourceTreeWidget->setCheckboxesVisible(false);
    ui->resourceTreeWidget->setGraphicsTweaks(Qn::HideLastRow | Qn::BackgroundOpacity | Qn::BypassGraphicsProxy);
    ui->resourceTreeWidget->setEditingEnabled();
//    ui->resourceTreeWidget->setFilterVisible(); //TODO: #gdm ask #Elric whether try to enable this =)

    ui->searchTreeWidget->setCheckboxesVisible(false);
    ui->searchTreeWidget->setGraphicsTweaks(Qn::HideLastRow | Qn::BackgroundOpacity | Qn::BypassGraphicsProxy);

    /* This is needed so that control's context menu is not embedded into the scene. */
    ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);

    m_renameAction = new QAction(this);

    setHelpTopic(this,                              Qn::MainWindow_Tree_Help);
    setHelpTopic(ui->searchTab,                     Qn::MainWindow_Tree_Search_Help);

    connect(ui->typeComboBox,       SIGNAL(currentIndexChanged(int)),   this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(textChanged(QString)),       this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(editingFinished()),          this,               SLOT(forceUpdateFilter()));
    connect(ui->clearFilterButton,  SIGNAL(clicked()),                  ui->filterLineEdit, SLOT(clear()));

    connect(ui->resourceTreeWidget, SIGNAL(activated(const QnResourcePtr &)),  this,        SIGNAL(activated(const QnResourcePtr &)));
    connect(ui->searchTreeWidget,   SIGNAL(activated(const QnResourcePtr &)),  this,        SIGNAL(activated(const QnResourcePtr &)));

    connect(ui->tabWidget,          SIGNAL(currentChanged(int)),        this,               SLOT(at_tabWidget_currentChanged(int)));
    connect(ui->resourceTreeWidget->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SIGNAL(selectionChanged()));

    /* Connect to context. */
    ui->resourceTreeWidget->setWorkbench(workbench());
    ui->searchTreeWidget->setWorkbench(workbench());

    connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),            this,   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this,   SLOT(at_workbench_currentLayoutChanged()));
    connect(workbench(),        SIGNAL(itemChanged(Qn::ItemRole)),                  this,   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(qnSettings->notifier(QnSettings::IP_SHOWN_IN_TREE), SIGNAL(valueChanged(int)), this, SLOT(at_showUrlsInTree_changed()));

    /* Run handlers. */
    updateFilter();

    at_showUrlsInTree_changed();
    at_workbench_currentLayoutChanged();
}

QnResourceBrowserWidget::~QnResourceBrowserWidget() {
    disconnect(workbench(), NULL, this, NULL);

    at_workbench_currentLayoutAboutToBeChanged();

    ui->searchTreeWidget->setWorkbench(NULL);
    ui->resourceTreeWidget->setWorkbench(NULL);
}

QPalette QnResourceBrowserWidget::comboBoxPalette() const {
    return ui->typeComboBox->palette();
}

void QnResourceBrowserWidget::setComboBoxPalette(const QPalette &palette) {
    ui->typeComboBox->setPalette(palette);
}

QnResourceBrowserWidget::Tab QnResourceBrowserWidget::currentTab() const {
    return static_cast<Tab>(ui->tabWidget->currentIndex());
}

void QnResourceBrowserWidget::setCurrentTab(Tab tab) {
    if(tab < 0 || tab >= TabCount) {
        qnWarning("Invalid resource tree widget tab '%1'.", static_cast<int>(tab));
        return;
    }

    ui->tabWidget->setCurrentIndex(tab);
}

bool QnResourceBrowserWidget::isLayoutSearchable(QnWorkbenchLayout *layout) const {
    return accessController()->permissions(layout->resource()) & Qn::WritePermission;
}

QnResourceSearchProxyModel *QnResourceBrowserWidget::layoutModel(QnWorkbenchLayout *layout, bool create) const {
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

QnResourceSearchSynchronizer *QnResourceBrowserWidget::layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const {
    QnResourceSearchSynchronizer *result = layout->property(qn_searchSynchronizerPropertyName).value<QnResourceSearchSynchronizer *>();
    if(create && result == NULL && isLayoutSearchable(layout)) {
        result = new QnResourceSearchSynchronizer(layout);
        result->setLayout(layout);
        result->setModel(layoutModel(layout, true));
        layout->setProperty(qn_searchSynchronizerPropertyName, QVariant::fromValue<QnResourceSearchSynchronizer *>(result));
    }
    return result;
}

QString QnResourceBrowserWidget::layoutFilter(QnWorkbenchLayout *layout) const {
    return layout->property(qn_filterPropertyName).toString();
}

void QnResourceBrowserWidget::setLayoutFilter(QnWorkbenchLayout *layout, const QString &filter) const {
    layout->setProperty(qn_filterPropertyName, filter);
}

void QnResourceBrowserWidget::killSearchTimer() {
    if (m_filterTimerId == 0) 
        return;

    killTimer(m_filterTimerId);
    m_filterTimerId = 0; 
}

void QnResourceBrowserWidget::showContextMenuAt(const QPoint &pos, bool ignoreSelection) {
    if(!context() || !context()->menu()) {
        qnWarning("Requesting context menu for a tree widget while no menu manager instance is available.");
        return;
    }
    QnActionManager *manager = context()->menu();

    QScopedPointer<QMenu> menu(manager->newMenu(Qn::TreeScope, ignoreSelection ? QnActionParameters() : QnActionParameters(currentTarget(Qn::TreeScope))));

    /* Add tree-local actions to the menu. */
    if(currentSelectionModel()->currentIndex().data(Qn::NodeTypeRole) != Qn::UsersNode || !currentSelectionModel()->selection().contains(currentSelectionModel()->currentIndex()) || ignoreSelection)
        manager->redirectAction(menu.data(), Qn::NewUserAction, NULL); /* Show 'New User' item only when clicking on 'Users' node. */ // TODO: implement with action parameters
    
    if(currentItemView() == ui->searchTreeWidget) {
        /* Disable rename action for search view. */
        manager->redirectAction(menu.data(), Qn::RenameAction, NULL);
    } else {
        manager->redirectAction(menu.data(), Qn::RenameAction, m_renameAction);
    }

    if(menu->isEmpty())
        return;

    /* Run menu. */
    QAction *action = menu->exec(pos);

    /* Process tree-local actions. */
    if(action == m_renameAction)
        currentItemView()->edit();
}

QnResourceTreeWidget *QnResourceBrowserWidget::currentItemView() const {
    if (ui->tabWidget->currentIndex() == ResourcesTab) {
        return ui->resourceTreeWidget;
    } else {
        return ui->searchTreeWidget;
    }
}

QItemSelectionModel *QnResourceBrowserWidget::currentSelectionModel() const {
    return currentItemView()->selectionModel();
}

QnResourceList QnResourceBrowserWidget::selectedResources() const {
    QnResourceList result;

    foreach (const QModelIndex &index, currentSelectionModel()->selectedRows()) {
        if (index.data(Qn::NodeTypeRole) == Qn::RecorderNode) {
            for (int i = 0; i < index.model()->rowCount(index); i++) {
                QModelIndex subIndex = index.model()->index(i, 0, index);
                QnResourcePtr resource = subIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
                if(resource && !result.contains(resource))
                    result.append(resource);
            }
        }

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource && !result.contains(resource))
            result.append(resource);
    }

    return result;
}

QnLayoutItemIndexList QnResourceBrowserWidget::selectedLayoutItems() const {
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

Qn::ActionScope QnResourceBrowserWidget::currentScope() const {
    return Qn::TreeScope;
}

QVariant QnResourceBrowserWidget::currentTarget(Qn::ActionScope scope) const {
    if(scope != Qn::TreeScope)
        return QVariant();

    QItemSelectionModel *selectionModel = currentSelectionModel();

    if(!selectionModel->currentIndex().data(Qn::ItemUuidRole).value<QUuid>().isNull()) { /* If it's a layout item. */
        return QVariant::fromValue(selectedLayoutItems());
    } else {
        return QVariant::fromValue(selectedResources());
    }
}

void QnResourceBrowserWidget::updateFilter(bool force) {
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
//        int pos = qMax(filter.lastIndexOf(QLatin1Char('+')), filter.lastIndexOf(QLatin1Char('\\'))) + 1;
        
        int pos = 0;
        /* Estimate size of the each term in filter expression. */
        while (pos < filter.size()){
            int size = 0;
            for(;pos < filter.size(); pos++){
                if (filter[pos] == QLatin1Char('+') || filter[pos] == QLatin1Char('\\'))
                    break;

                if(!filter[pos].isSpace())
                    size++;
            }
            pos++;
            if (size > 0 && size < 3)
                return; /* Filter too short, ignore. */
        }
    }

    m_filterTimerId = startTimer(filter.isEmpty() ? 0 : 300);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceBrowserWidget::contextMenuEvent(QContextMenuEvent *event) {
    QWidget *child = childAt(event->pos());
    while(child && child != ui->resourceTreeWidget && child != ui->searchTreeWidget)
        child = child->parentWidget();

    /** 
     * Note that we cannot use event->globalPos() here as it doesn't work when
     * the widget is embedded into graphics scene.
     */
    showContextMenuAt(QCursor::pos(), !child);
}

void QnResourceBrowserWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the tree widget. */
}

void QnResourceBrowserWidget::mousePressEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceBrowserWidget::mouseReleaseEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceBrowserWidget::keyPressEvent(QKeyEvent *event) {
    event->accept();
    if (event->key() == Qt::Key_Menu) {
        if (ui->filterLineEdit->hasFocus())
            return;

        QPoint pos = currentItemView()->selectionPos();
        if (pos.isNull())
            return;
        showContextMenuAt(display()->view()->mapToGlobal(pos));
    }
}

void QnResourceBrowserWidget::keyReleaseEvent(QKeyEvent *event) {
    event->accept();
}

void QnResourceBrowserWidget::timerEvent(QTimerEvent *event) {
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
            model->addCriterion(QnResourceCriterion(QnResource::user));
            model->addCriterion(QnResourceCriterion(QnResource::layout));
        }
    }

    QWidget::timerEvent(event);
}

void QnResourceBrowserWidget::at_workbench_currentLayoutAboutToBeChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    disconnect(layout, NULL, this, NULL);

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->disableUpdates();
    setLayoutFilter(layout, ui->filterLineEdit->text());

    QnScopedValueRollback<bool> guard(&m_ignoreFilterChanges, true);
    ui->searchTreeWidget->setModel(NULL);
    ui->filterLineEdit->setText(QString());
    killSearchTimer();
}

void QnResourceBrowserWidget::at_workbench_currentLayoutChanged() {
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

void QnResourceBrowserWidget::at_workbench_itemChanged(Qn::ItemRole /*role*/) {
    /* Raised state has changed. */
    ui->resourceTreeWidget->update();
}

void QnResourceBrowserWidget::at_layout_itemAdded(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceBrowserWidget::at_layout_itemRemoved(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceBrowserWidget::at_tabWidget_currentChanged(int index) {
    if(index == SearchTab) {
        QnWorkbenchLayout *layout = workbench()->currentLayout();

        layoutSynchronizer(layout, true); /* Just initialize the synchronizer. */
        QnResourceSearchProxyModel *model = layoutModel(layout, true);

        ui->searchTreeWidget->setModel(model);
        ui->searchTreeWidget->expandAll();

        /* View re-creates selection model for each model that is supplied to it, 
         * so we have to re-connect each time the model changes. */
        connect(ui->searchTreeWidget->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SIGNAL(selectionChanged()), Qt::UniqueConnection);
    }

    emit currentTabChanged();
}



void QnResourceBrowserWidget::at_showUrlsInTree_changed() {
    bool urlsShown = qnSettings->isIpShownInTree();

    m_resourceModel->setUrlsShown(urlsShown);
}

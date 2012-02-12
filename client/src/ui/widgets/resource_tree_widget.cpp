#include "resource_tree_widget.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>
#include <QWheelEvent>
#include <QStyledItemDelegate>

#include <utils/common/scoped_value_rollback.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/video_server.h>
#include "ui/context_menu_helper.h"
#include "ui/device_settings/dlg_factory.h"
#include "ui/dialogs/tagseditdialog.h"
#include "ui/dialogs/camerasettingsdialog.h"
#include "ui/dialogs/serversettingsdialog.h"
#include "ui/models/resource_model.h"
#include "ui/models/resource_search_proxy_model.h"
#include "ui/models/resource_search_synchronizer.h"
#include "ui/style/skin.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench_layout.h"
#include "youtube/youtubeuploaddialog.h"
#include "file_processor.h"

Q_DECLARE_METATYPE(QnResourceSearchProxyModel *);
Q_DECLARE_METATYPE(QnResourceSearchSynchronizer *);

namespace {
    const char *qn_searchModelPropertyName = "_qn_searchModel";
    const char *qn_searchSynchronizerPropertyName = "_qn_searchSynchronizer";
    const char *qn_searchStringPropertyName = "_qn_searchString";
    const char *qn_searchCriterionPropertyName = "_qn_searchCriterion";
}

class QnResourceTreeItemDelegate: public QStyledItemDelegate {
public:
    explicit QnResourceTreeItemDelegate(QnResourceTreeWidget *parent): QStyledItemDelegate(parent) {
        Q_ASSERT(parent);
    }

protected:
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);

        QnResourceTreeWidget *navTree = static_cast<QnResourceTreeWidget *>(parent());
        if (navTree->m_workbench && navTree->m_workbench->currentLayout()) {
            if (const QnResourceModel *model = qobject_cast<const QnResourceModel *>(index.model())) {
                QnResourcePtr resource = model->resource(index);
                if (resource && !navTree->m_workbench->currentLayout()->items(resource->getUniqueId()).isEmpty())
                    option->font.setBold(true);
            }
        }
    }
};


QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent): 
    QWidget(parent),
    m_filterTimerId(0),
    m_workbench(NULL),
    m_ignoreFilterChanges(false)
{
    m_previousItemButton = new QToolButton(this);
    m_previousItemButton->setText(QLatin1String("<"));
    m_previousItemButton->setToolTip(tr("Previous Item"));
    m_previousItemButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_previousItemButton->setIcon(Skin::icon(QLatin1String("left-arrow.png"))); // ###
    m_previousItemButton->hide(); // ###

    m_nextItemButton = new QToolButton(this);
    m_nextItemButton->setText(QLatin1String(">"));
    m_nextItemButton->setToolTip(tr("Next Item"));
    m_nextItemButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_nextItemButton->setIcon(Skin::icon(QLatin1String("right-arrow.png"))); // ###
    m_nextItemButton->hide(); // ###


    m_filterLineEdit = new QLineEdit(this);
    m_filterLineEdit->setPlaceholderText(tr("Search"));

    m_clearFilterButton = new QToolButton(this);
    m_clearFilterButton->setText(QLatin1String("X"));
    m_clearFilterButton->setToolTip(tr("Reset Filter"));
    m_clearFilterButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_clearFilterButton->setIcon(Skin::icon(QLatin1String("clear.png")));
    m_clearFilterButton->setIconSize(QSize(16, 16));
    m_clearFilterButton->setVisible(!m_filterLineEdit->text().isEmpty());

    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_filterLineEdit_textChanged(QString)));
    connect(m_clearFilterButton, SIGNAL(clicked()), m_filterLineEdit, SLOT(clear()));

    m_resourceModel = new QnResourceModel(this);
    m_resourceModel->setResourcePool(qnResPool);

    m_resourceTreeView = new QTreeView(this);
    m_resourceTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resourceTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resourceTreeView->setAllColumnsShowFocus(true);
    m_resourceTreeView->setRootIsDecorated(true);
    m_resourceTreeView->setHeaderHidden(true); // ###
    m_resourceTreeView->setSortingEnabled(false); // ###
    m_resourceTreeView->setUniformRowHeights(true);
    m_resourceTreeView->setWordWrap(false);
    m_resourceTreeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_resourceTreeView->setItemDelegate(new QnResourceTreeItemDelegate(this));

    connect(m_resourceTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(at_treeView_activated(QModelIndex)));

    m_searchTreeView = new QTreeView(this);
    m_searchTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_searchTreeView->setAllColumnsShowFocus(true);
    m_searchTreeView->setRootIsDecorated(true);
    m_searchTreeView->setHeaderHidden(true); // ###
    m_searchTreeView->setSortingEnabled(false); // ###
    m_searchTreeView->setUniformRowHeights(true);
    m_searchTreeView->setWordWrap(false);
    m_searchTreeView->setDragDropMode(QAbstractItemView::DragOnly); // ###

    connect(m_searchTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(at_treeView_activated(QModelIndex)));


    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(3);
    topLayout->addWidget(m_previousItemButton);
    topLayout->addWidget(m_nextItemButton);
    topLayout->addWidget(m_filterLineEdit);
    topLayout->addWidget(m_clearFilterButton);

    QWidget *searchTab = new QWidget(this);

    QVBoxLayout *searchTabLayout = new QVBoxLayout;
    searchTabLayout->setContentsMargins(0, 0, 0, 0);
    searchTabLayout->addLayout(topLayout);
    searchTabLayout->addWidget(m_searchTreeView);
    searchTab->setLayout(searchTabLayout);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_resourceTreeView, tr("Resources"));
    m_tabWidget->addTab(searchTab, tr("Search"));

    connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(at_tabWidget_currentChanged(int)));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);

    setMinimumWidth(180);
    setMaximumWidth(300);


    m_resourceTreeView->setModel(m_resourceModel);
    //QMetaObject::invokeMethod(m_resourceTreeView, "expandAll", Qt::QueuedConnection); // ###
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    return;
}

void QnResourceTreeWidget::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL) {
        disconnect(m_workbench, NULL, this, NULL);

        at_workbench_currentLayoutAboutToBeChanged();

        m_filterLineEdit->setEnabled(false);
    }

    m_workbench = workbench;

    if(m_workbench) {
        m_filterLineEdit->setEnabled(true);

        at_workbench_currentLayoutChanged();

        connect(m_workbench, SIGNAL(currentLayoutAboutToBeChanged()),   this, SLOT(at_workbench_currentLayoutAboutToBeChanged()));
        connect(m_workbench, SIGNAL(currentLayoutChanged()),            this, SLOT(at_workbench_currentLayoutChanged()));
        connect(m_workbench, SIGNAL(aboutToBeDestroyed()),              this, SLOT(at_workbench_aboutToBeDestroyed()));
    }
}

void QnResourceTreeWidget::open() {
    QAbstractItemView *view = m_tabWidget->currentIndex() == 0 ? m_resourceTreeView : m_searchTreeView;
    foreach (const QModelIndex &index, view->selectionModel()->selectedRows())
        at_treeView_activated(index);
}

QnResourceSearchProxyModel *QnResourceTreeWidget::layoutModel(QnWorkbenchLayout *layout, bool create) const {
    QnResourceSearchProxyModel *result = layout->property(qn_searchModelPropertyName).value<QnResourceSearchProxyModel *>();
    if(create && result == NULL) {
        result = new QnResourceSearchProxyModel(layout);
        result->setFilterCaseSensitivity(Qt::CaseInsensitive);
        result->setFilterKeyColumn(0);
        result->setFilterRole(Qn::SearchStringRole);
        result->setSortCaseSensitivity(Qt::CaseInsensitive);
        result->setDynamicSortFilter(true);
        result->setSourceModel(m_resourceModel);
        layout->setProperty(qn_searchModelPropertyName, QVariant::fromValue<QnResourceSearchProxyModel *>(result));

        QnResourceCriterion *criterion = new QnResourceCriterion();
        result->addCriterion(criterion);
        setLayoutCriterion(layout, criterion);

        /* Add default criteria. */
        result->addCriterion(new QnResourceCriterion(QnResource::server)); /* Always accept servers. */
    }
    return result;
}

QnResourceSearchSynchronizer *QnResourceTreeWidget::layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const {
    QnResourceSearchSynchronizer *result = layout->property(qn_searchSynchronizerPropertyName).value<QnResourceSearchSynchronizer *>();
    if(create && result == NULL) {
        result = new QnResourceSearchSynchronizer(layout);
        result->setLayout(layout);
        result->setModel(layoutModel(layout, true));
        layout->setProperty(qn_searchSynchronizerPropertyName, QVariant::fromValue<QnResourceSearchSynchronizer *>(result));
    }
    return result;
}

QString QnResourceTreeWidget::layoutSearchString(QnWorkbenchLayout *layout) const {
    return layout->property(qn_searchStringPropertyName).toString();
}

void QnResourceTreeWidget::setLayoutSearchString(QnWorkbenchLayout *layout, const QString &searchString) const {
    layout->setProperty(qn_searchStringPropertyName, searchString);
}

QnResourceCriterion *QnResourceTreeWidget::layoutCriterion(QnWorkbenchLayout *layout) const {
    return layout->property(qn_searchCriterionPropertyName).value<QnResourceCriterion *>();
}

void QnResourceTreeWidget::setLayoutCriterion(QnWorkbenchLayout *layout, QnResourceCriterion *criterion) const {
    layout->setProperty(qn_searchCriterionPropertyName, QVariant::fromValue<QnResourceCriterion *>(criterion));
}

void QnResourceTreeWidget::killSearchTimer() {
    if (m_filterTimerId == 0) 
        return;

    killTimer(m_filterTimerId);
    m_filterTimerId = 0; 
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeWidget::contextMenuEvent(QContextMenuEvent *) {
    QnResourceList resources;

    QItemSelectionModel *selectionModel;
    if (m_tabWidget->currentIndex() == 0) {
        selectionModel = m_resourceTreeView->selectionModel();
    } else {
        selectionModel = m_searchTreeView->selectionModel();
    }
    foreach (const QModelIndex &index, selectionModel->selectedRows())
        resources.append(index.data(Qn::ResourceRole).value<QnResourcePtr>());

    QScopedPointer<QMenu> menu(new QMenu);

    QAction *openAction = new QAction(tr("Open"), menu.data());
    connect(openAction, SIGNAL(triggered()), this, SLOT(open()));
    menu->addAction(openAction);
    menu->addSeparator();
    if (resources.size() == 1) {
        const QnResourcePtr &resource = resources.first();
        if (resource->checkFlags(QnResource::video) || resource->checkFlags(QnResource::SINGLE_SHOT)) {
            menu->addAction(&cm_editTags);

            if (resource->associatedWithFile())
                menu->addAction(&cm_remove_from_disk);

            if (resource->checkFlags(QnResource::ARCHIVE))
                menu->addAction(&cm_upload_youtube);

            if (resource->checkFlags(QnResource::ARCHIVE) || resource->checkFlags(QnResource::SINGLE_SHOT))
                menu->addAction(&cm_open_containing_folder);
        } else if (resource->checkFlags(QnResource::live_cam)) {
            // ### Start/Stop recording (Ctrl+R)
            // ### Delete(if no connection)(Shift+Del)
            menu->addAction(&cm_editTags);
            // ### Export selected... (Ctrl+E)
        } else if (resource->checkFlags(QnResource::server)) {
            // ### New camera
        }
        // ### handle layouts

        if ((CLDeviceSettingsDlgFactory::canCreateDlg(resource) && resource.dynamicCast<QnVirtualCameraResource>()) || resource.dynamicCast<QnVideoServerResource>())
            menu->addAction(&cm_settings);
    } else if (resources.size() > 1) {
        // ###
    } else {
        //menu->addAction(&cm_open_file);
        //menu->addAction(&cm_new_item);
    }
    menu->addSeparator();
    //menu->addAction(&cm_toggle_fullscreen);
    menu->addAction(&cm_screen_recording);
    //menu->addAction(&cm_preferences);
    /*menu->addSeparator();
    menu->addAction(&cm_exit);*/

    QAction *action = menu->exec(QCursor::pos());
    if (!action)
        return;

    if (resources.size() == 1) {
        const QnResourcePtr &resource = resources.first();
        if (action == &cm_remove_from_disk) {
            QnFileProcessor::deleteLocalResources(QnResourceList() << resource);
        } else if (action == &cm_settings) { // ### move to app-global scope ?
            if (resource.dynamicCast<QnVirtualCameraResource>())
            {
                CameraSettingsDialog dialog(QApplication::activeWindow(), resource.dynamicCast<QnVirtualCameraResource>());
                dialog.setWindowModality(Qt::ApplicationModal);
                dialog.exec();
            } else if (resource.dynamicCast<QnVideoServerResource>())
            {
                ServerSettingsDialog dialog(QApplication::activeWindow(), resource.dynamicCast<QnVideoServerResource>());
                dialog.setWindowModality(Qt::ApplicationModal);
                dialog.exec();
            }
        } else if (action == &cm_editTags) { // ### move to app-global scope ?
            TagsEditDialog dialog(QStringList() << resource->getUniqueId(), QApplication::activeWindow());
            dialog.setWindowModality(Qt::ApplicationModal);
            dialog.exec();
        } else if (action == &cm_upload_youtube) { // ### move to app-global scope ?
            YouTubeUploadDialog dialog(resource, QApplication::activeWindow());
            dialog.setWindowModality(Qt::ApplicationModal);
            dialog.exec();
        }
    }
}

void QnResourceTreeWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the tree widget. */
}

void QnResourceTreeWidget::mousePressEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceTreeWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_filterTimerId) {
        killTimer(m_filterTimerId);
        m_filterTimerId = 0;

        if (m_workbench) {
            QnWorkbenchLayout *layout = m_workbench->currentLayout();
            QnResourceSearchProxyModel *model = layoutModel(layout, true);
            
            QString filter = m_filterLineEdit->text();

            QnResourceCriterion *oldCriterion = layoutCriterion(layout);
            QnResourceCriterion *newCriterion = new QnResourceCriterionGroup(filter);

            model->replaceCriterion(oldCriterion, newCriterion);

            delete oldCriterion;
            setLayoutCriterion(layout, newCriterion);
        }
    }

    QWidget::timerEvent(event);
}

void QnResourceTreeWidget::at_workbench_currentLayoutAboutToBeChanged() {
    QnWorkbenchLayout *layout = m_workbench->currentLayout();

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->disableUpdates();
    setLayoutSearchString(layout, m_filterLineEdit->text());

    QnScopedValueRollback<bool> guard(&m_ignoreFilterChanges, true);
    m_searchTreeView->setModel(NULL);
    m_filterLineEdit->setText(QString());
    killSearchTimer();
}

void QnResourceTreeWidget::at_workbench_currentLayoutChanged() {
    QnWorkbenchLayout *layout = m_workbench->currentLayout();

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->enableUpdates();

    at_tabWidget_currentChanged(m_tabWidget->currentIndex());

    QnScopedValueRollback<bool> guard(&m_ignoreFilterChanges, true);
    m_filterLineEdit->setText(layoutSearchString(layout));
}

void QnResourceTreeWidget::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnResourceTreeWidget::at_tabWidget_currentChanged(int index) {
    if(index != 1)
        return;

    QnWorkbenchLayout *layout = m_workbench->currentLayout();

    layoutSynchronizer(layout, true); /* Just initialize the synchronizer. */
    QnResourceSearchProxyModel *model = layoutModel(layout, true);

    m_searchTreeView->setModel(model);
    m_searchTreeView->expandAll();
}

void QnResourceTreeWidget::at_filterLineEdit_textChanged(const QString &filter) {
    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        m_filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    m_clearFilterButton->setVisible(!filter.isEmpty());
    killSearchTimer();

    if(!m_workbench)
        return;

    if(m_ignoreFilterChanges)
        return;

    if (!filter.isEmpty() && filter.size() < 3) 
        return; /* Filter too short, ignore. */

    m_filterTimerId = startTimer(filter.isEmpty() ? 0 : 300);
}

void QnResourceTreeWidget::at_treeView_activated(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        emit activated(resource);
}
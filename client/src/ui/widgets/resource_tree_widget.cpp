#include "resource_tree_widget.h"

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QLineEdit>
#include <QtGui/QMenu>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>

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
#include "ui/style/skin.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench_layout.h"
#include "youtube/youtubeuploaddialog.h"

#include "file_processor.h"


#include <QtGui/QStyledItemDelegate>

class QnResourceTreeItemDelegate : public QStyledItemDelegate
{
public:
    explicit QnResourceTreeItemDelegate(QnResourceTreeWidget *parent)
        : QStyledItemDelegate(parent)
    {
        Q_ASSERT(parent);
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
    {
        QStyledItemDelegate::initStyleOption(option, index);

        QnResourceTreeWidget *navTree = static_cast<QnResourceTreeWidget *>(parent());
        if (navTree->m_controller && navTree->m_controller->layout()) {
            if (const QnResourceModel *model = qobject_cast<const QnResourceModel *>(index.model())) {
                if (!navTree->m_controller->layout()->items(model->resource(index)->getUniqueId()).isEmpty())
                    option->font.setBold(true);
            }
        }
/*        if (navTree->m_searchProxyModel) {
            const QModelIndex proxyIndex = navTree->m_searchProxyModel->mapFromSource(index);
            if (proxyIndex.isValid())
                option->font.setBold(true);
        }*/
    }
};


QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent)
    : QWidget(parent),
      m_filterTimerId(0),
      m_dontSyncWithLayout(false)
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


    m_resourcesModel = new QnResourceModel(this);

    m_resourcesTreeView = new QTreeView(this);
    m_resourcesTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resourcesTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resourcesTreeView->setAllColumnsShowFocus(true);
    m_resourcesTreeView->setRootIsDecorated(true);
    m_resourcesTreeView->setHeaderHidden(true); // ###
    m_resourcesTreeView->setSortingEnabled(false); // ###
    m_resourcesTreeView->setUniformRowHeights(true);
    m_resourcesTreeView->setWordWrap(false);
    m_resourcesTreeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_resourcesTreeView->setItemDelegate(new QnResourceTreeItemDelegate(this));

    connect(m_resourcesTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(at_treeView_activated(QModelIndex)));

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
    m_tabWidget->addTab(m_resourcesTreeView, tr("Resources"));
    m_tabWidget->addTab(searchTab, tr("Search"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);

    setMinimumWidth(180);
    setMaximumWidth(300);


    m_resourcesTreeView->setModel(m_resourcesModel);
    //QMetaObject::invokeMethod(m_resourcesTreeView, "expandAll", Qt::QueuedConnection); // ###
}

QnResourceTreeWidget::~QnResourceTreeWidget()
{
}

void QnResourceTreeWidget::setWorkbenchController(QnWorkbenchController *controller)
{
    if (m_controller) {
        disconnect(m_controller->workbench(), SIGNAL(currentLayoutAboutToBeChanged()), this, SLOT(workbenchLayoutAboutToBeChanged()));
        disconnect(m_controller->workbench(), SIGNAL(currentLayoutChanged()), this, SLOT(workbenchLayoutChanged()));
    }

    workbenchLayoutAboutToBeChanged();
    m_controller = controller;
    m_searchProxyModel = 0;
    workbenchLayoutChanged();

    if (m_controller) {
        connect(m_controller->workbench(), SIGNAL(currentLayoutAboutToBeChanged()), this, SLOT(workbenchLayoutAboutToBeChanged()));
        connect(m_controller->workbench(), SIGNAL(currentLayoutChanged()), this, SLOT(workbenchLayoutChanged()));
    }
}

void QnResourceTreeWidget::workbenchLayoutAboutToBeChanged()
{
    if (!m_controller || !m_controller->layout())
        return;

    QnWorkbenchLayout *layout = m_controller->layout();

    if (m_searchProxyModel) {
        disconnect(m_searchProxyModel.data(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleInsertRows(QModelIndex,int,int)));
        disconnect(m_searchProxyModel.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), this, SLOT(handleRemoveRows(QModelIndex,int,int)));
    }

    disconnect(layout, SIGNAL(itemAdded(QnWorkbenchItem*)), this, SLOT(workbenchLayoutItemAdded(QnWorkbenchItem*)));
    disconnect(layout, SIGNAL(itemRemoved(QnWorkbenchItem*)), this, SLOT(workbenchLayoutItemRemoved(QnWorkbenchItem*)));
}

Q_DECLARE_METATYPE(QnResourceSearchProxyModel *) // ###

void QnResourceTreeWidget::workbenchLayoutChanged()
{
    if (!m_controller || !m_controller->layout())
        return;

    QnWorkbenchLayout *layout = m_controller->layout();

    connect(layout, SIGNAL(itemAdded(QnWorkbenchItem*)), this, SLOT(workbenchLayoutItemAdded(QnWorkbenchItem*)));
    connect(layout, SIGNAL(itemRemoved(QnWorkbenchItem*)), this, SLOT(workbenchLayoutItemRemoved(QnWorkbenchItem*)));

    m_searchProxyModel = layout->property("model").value<QnResourceSearchProxyModel *>(); // ###
    if (!m_searchProxyModel) {
        QnResourceSearchProxyModel *proxyModel = new QnResourceSearchProxyModel(layout);
        proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxyModel->setFilterKeyColumn(0);
        proxyModel->setFilterRole(QnResourceModel::SearchStringRole);
        proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        proxyModel->setDynamicSortFilter(true);
        m_searchProxyModel = proxyModel;
        layout->setProperty("model", QVariant::fromValue(proxyModel)); // ###
    }

    connect(m_searchProxyModel.data(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleInsertRows(QModelIndex,int,int)));
    connect(m_searchProxyModel.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), this, SLOT(handleRemoveRows(QModelIndex,int,int)));

    m_searchTreeView->setModel(m_searchProxyModel);
    m_searchTreeView->expandAll();

    disconnect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_filterLineEdit_textChanged(QString)));
    m_filterLineEdit->setText(m_searchProxyModel->filterRegExp().pattern()); // ###
    m_clearFilterButton->setVisible(!m_filterLineEdit->text().isEmpty());
    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_filterLineEdit_textChanged(QString)));
}

void QnResourceTreeWidget::workbenchLayoutItemAdded(QnWorkbenchItem *item)
{
    const QnResourcePtr &resource = qnResPool->getResourceByUniqId(item->resourceUid());

    m_resourcesTreeView->update(m_resourcesModel->index(resource));

    if (!m_dontSyncWithLayout && !m_filterLineEdit->text().isEmpty()) {
        const QModelIndex index = m_searchProxyModel->indexFromResource(resource);
        if (!index.isValid()) {
            // ### improve/optimize
            QString subFilter = QLatin1String("id:") + resource->getId().toString();
            QString filter = m_filterLineEdit->text();
            filter.replace(QLatin1Char('-') + subFilter, QString());
            if (!filter.contains(QLatin1Char('+') + subFilter))
                filter += QLatin1Char('+') + subFilter;
            m_filterLineEdit->setText(filter);
            // ###
        } else if (m_searchTreeView->isVisible()) {
            m_searchTreeView->update(index);
        }
    }
}

void QnResourceTreeWidget::workbenchLayoutItemRemoved(QnWorkbenchItem *item)
{
    const QnResourcePtr &resource = qnResPool->getResourceByUniqId(item->resourceUid());

    if(!m_controller->layout()->items(resource->getUniqueId()).isEmpty())
        return;

    m_resourcesTreeView->update(m_resourcesModel->index(resource));

    if (!m_dontSyncWithLayout && !m_filterLineEdit->text().isEmpty()) {
        const QModelIndex index = m_searchProxyModel->indexFromResource(resource);
        if (index.isValid()) {
            // ### improve/optimize
            QString subFilter = QLatin1String("id:") + resource->getId().toString();
            QString filter = m_filterLineEdit->text();
            filter.replace(QLatin1Char('+') + subFilter, QString());
            if (!filter.contains(QLatin1Char('-') + subFilter))
                filter += QLatin1Char('-') + subFilter;
            m_filterLineEdit->setText(filter);
            // ###
        }
    }
}

void QnResourceTreeWidget::handleInsertRows(const QModelIndex &parent, int first, int last)
{
    if (!m_controller || !m_searchProxyModel)
        return;

    for (int row = first; row <= last; ++row) {
        const QModelIndex index = m_searchProxyModel->index(row, 0, parent);
        QnResourcePtr resource = m_searchProxyModel->resourceFromIndex(index);

        if(m_controller->display()->widget(resource) == NULL)
            m_controller->drop(resource);
    }
}

void QnResourceTreeWidget::handleRemoveRows(const QModelIndex &parent, int first, int last)
{
    if (!m_controller || !m_searchProxyModel)
        return;

    for (int row = first; row <= last; ++row) {
        const QModelIndex index = m_searchProxyModel->index(row, 0, parent);
        QnResourcePtr resource = m_searchProxyModel->resourceFromIndex(index);
        m_controller->remove(resource);
    }
}

void QnResourceTreeWidget::contextMenuEvent(QContextMenuEvent *)
{
    QnResourceList resources;
    if (m_tabWidget->currentIndex() == 0) {
        foreach (const QModelIndex &index, m_resourcesTreeView->selectionModel()->selectedRows())
            //resources.append(m_resourcesModel->resourceFromIndex(index));
            resources.append(m_resourcesModel->resource(index));
    } else /*if (m_tabWidget->currentIndex() == 1)*/ {
        foreach (const QModelIndex &index, m_searchTreeView->selectionModel()->selectedRows())
            resources.append(m_searchProxyModel->resourceFromIndex(index));
    }

    QScopedPointer<QMenu> menu(new QMenu);

    QAction *openAction = new QAction(tr("Open"), menu.data());
    connect(openAction, SIGNAL(triggered()), this, SLOT(open()));
    menu->addAction(openAction);
    menu->addSeparator();
    if (resources.size() == 1) {
        const QnResourcePtr &resource = resources.first();
        if (resource->checkFlag(QnResource::video) || resource->checkFlag(QnResource::SINGLE_SHOT)) {
            menu->addAction(&cm_editTags);

            if (resource->associatedWithFile())
                menu->addAction(&cm_remove_from_disk);

            if (resource->checkFlag(QnResource::ARCHIVE))
                menu->addAction(&cm_upload_youtube);

            if (resource->checkFlag(QnResource::ARCHIVE) || resource->checkFlag(QnResource::SINGLE_SHOT))
                menu->addAction(&cm_open_containing_folder);
        } else if (resource->checkFlag(QnResource::live_cam)) {
            // ### Start/Stop recording (Ctrl+R)
            // ### Delete(if no connection)(Shift+Del)
            menu->addAction(&cm_editTags);
            // ### Export selected... (Ctrl+E)
        } else if (resource->checkFlag(QnResource::server)) {
            // ### New camera
        }
        // ### handle layouts

        if ((CLDeviceSettingsDlgFactory::canCreateDlg(resource) && resource.dynamicCast<QnVirtualCameraResource>()) || resource.dynamicCast<QnVideoServerResource>())
            menu->addAction(&cm_settings);
    } else if (resources.size() > 1) {
        // ###
    } else {
        menu->addAction(&cm_open_file);
        menu->addAction(&cm_new_item);
    }
    menu->addSeparator();
    menu->addAction(&cm_toggle_fullscreen);
    menu->addAction(&cm_screen_recording);
    menu->addAction(&cm_preferences);
    menu->addSeparator();
    menu->addAction(&cm_exit);

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

void QnResourceTreeWidget::wheelEvent(QWheelEvent *event)
{
    event->accept(); /* Do not propagate wheel events past the tree widget. */
}

void QnResourceTreeWidget::mousePressEvent(QMouseEvent *event)
{
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceTreeWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_filterTimerId) {
        killTimer(m_filterTimerId);
        m_filterTimerId = 0;

        if (m_controller) {
            if (QnWorkbenchLayout *layout = m_controller->layout())
                layout->setProperty("caption", m_filterLineEdit->text()); // ### unescape, normalize, etc.
        }

        if (m_searchProxyModel) {
            m_dontSyncWithLayout = true;
            m_searchProxyModel->setFilterWildcard(m_filterLineEdit->text());
            m_dontSyncWithLayout = false;
            m_searchTreeView->expandAll();
        }
    }

    QWidget::timerEvent(event);
}

void QnResourceTreeWidget::at_filterLineEdit_textChanged(const QString &filter)
{
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        m_filterLineEdit->clear();
        return;
    }

    m_clearFilterButton->setVisible(!filter.isEmpty());

    if (m_controller) {
        if (QnWorkbenchLayout *layout = m_controller->layout()) {
            // open a new tab, if needed
            if (!layout->isEmpty() && layout->property("caption").toString().isEmpty())
                Q_EMIT newTabRequested();
        }
    }

    if (m_searchProxyModel && m_searchProxyModel->sourceModel() != m_resourcesModel)
        m_searchProxyModel->setSourceModel(m_resourcesModel);

    if (m_filterTimerId != 0)
        killTimer(m_filterTimerId);
    m_filterTimerId = 0; if (!filter.isEmpty() && filter.size() < 3) return; // ### remove after resource_widget initialization fixup
    m_filterTimerId = startTimer(!filter.isEmpty() ? qMax(1000 - filter.size() * 100, 0) : 0);
}

void QnResourceTreeWidget::at_treeView_activated(const QModelIndex &index)
{
    QnResourcePtr resource;
    if (const QnResourceModel *model = qobject_cast<const QnResourceModel *>(index.model()))
        resource = model->resource(index);
    else if (const QnResourceSearchProxyModel *model = qobject_cast<const QnResourceSearchProxyModel *>(index.model()))
        resource = model->resourceFromIndex(index);
    if (resource)
        Q_EMIT activated(resource->getId());
}

void QnResourceTreeWidget::open()
{
    QAbstractItemView *view = m_tabWidget->currentIndex() == 0 ? m_resourcesTreeView : m_searchTreeView;
    foreach (const QModelIndex &index, view->selectionModel()->selectedRows())
        at_treeView_activated(index);
}

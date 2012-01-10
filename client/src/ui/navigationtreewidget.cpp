#include "navigationtreewidget.h"

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

#include "ui/context_menu_helper.h"
#include "ui/device_settings/dlg_factory.h"
#include "ui/dialogs/tagseditdialog.h"
#include "ui/models/resourcemodel.h"
#include "ui/skin/skin.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_layout.h"
#include "youtube/youtubeuploaddialog.h"


#include <QtGui/QStyledItemDelegate>

class NavigationTreeItemDelegate : public QStyledItemDelegate
{
public:
    explicit NavigationTreeItemDelegate(NavigationTreeWidget *parent)
        : QStyledItemDelegate(parent)
    {
        Q_ASSERT(parent);
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
    {
        QStyledItemDelegate::initStyleOption(option, index);

        NavigationTreeWidget *navTree = static_cast<NavigationTreeWidget *>(parent());
        if (navTree->m_controller) {
            QnResourcePtr resource = qnResPool->getResourceById(index.data(Qt::UserRole + 1));
            if (navTree->m_controller->layout()->items().contains(navTree->m_controller->item(resource)))
                option->font.setBold(true);
        }
/*        if (navTree->m_searchProxyModel) {
            const QModelIndex proxyIndex = navTree->m_searchProxyModel->mapFromSource(index);
            if (proxyIndex.isValid())
                option->font.setBold(true);
        }*/
    }
};


NavigationTreeWidget::NavigationTreeWidget(QWidget *parent)
    : QWidget(parent),
      m_searchProxyModel(0),
      m_controller(0)
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
    m_filterLineEdit->setMaxLength(150);

    m_clearFilterButton = new QToolButton(this);
    m_clearFilterButton->setText(QLatin1String("X"));
    m_clearFilterButton->setToolTip(tr("Reset Filter"));
    m_clearFilterButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_clearFilterButton->setIcon(Skin::icon(QLatin1String("clear.png")));
    m_clearFilterButton->setIconSize(QSize(16, 16));
    m_clearFilterButton->setVisible(!m_filterLineEdit->text().isEmpty());

    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(m_clearFilterButton, SIGNAL(clicked()), m_filterLineEdit, SLOT(clear()));

    m_filterTimerId = 0;


    m_resourcesModel = new ResourceModel(this);

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
    m_resourcesTreeView->setItemDelegate(new NavigationTreeItemDelegate(this));

    connect(m_resourcesTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(itemActivated(QModelIndex)));

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

    connect(m_searchTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(itemActivated(QModelIndex)));

    m_resourcesTreeView->setModel(m_resourcesModel);
    //QMetaObject::invokeMethod(m_resourcesTreeView, "expandAll", Qt::QueuedConnection); // ###


    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(3);
    topLayout->addWidget(m_previousItemButton);
    topLayout->addWidget(m_nextItemButton);
    topLayout->addWidget(m_filterLineEdit);
    topLayout->addWidget(m_clearFilterButton);
#if 1
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
#else
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_resourcesTreeView);
    setLayout(mainLayout);
#endif

    setMinimumWidth(180);
    setMaximumWidth(350);

    setAcceptDrops(true);
}

NavigationTreeWidget::~NavigationTreeWidget()
{
}

void NavigationTreeWidget::setWorkbenchController(QnWorkbenchController *controller)
{
    if (m_controller) {
        disconnect(m_controller->workbench(), SIGNAL(layoutAboutToBeChanged()), this, SLOT(workbenchLayoutAboutToBeChanged()));
        disconnect(m_controller->workbench(), SIGNAL(layoutChanged()), this, SLOT(workbenchLayoutChanged()));
    }

    workbenchLayoutAboutToBeChanged();
    m_controller = controller;
    m_searchProxyModel = 0;
    workbenchLayoutChanged();

    if (m_controller) {
        connect(m_controller->workbench(), SIGNAL(layoutAboutToBeChanged()), this, SLOT(workbenchLayoutAboutToBeChanged()));
        connect(m_controller->workbench(), SIGNAL(layoutChanged()), this, SLOT(workbenchLayoutChanged()));
    }
}

void NavigationTreeWidget::workbenchLayoutAboutToBeChanged()
{
    if (!m_controller)
        return;

    if (m_searchProxyModel) {
        disconnect(m_searchProxyModel.data(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleInsertRows(QModelIndex,int,int)));
        disconnect(m_searchProxyModel.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), this, SLOT(handleRemoveRows(QModelIndex,int,int)));
    }

    disconnect(m_controller->layout(), SIGNAL(itemAdded(QnWorkbenchItem*)), m_resourcesTreeView, SLOT(clearSelection())); // ### update(QModelIndex)
    disconnect(m_controller->layout(), SIGNAL(itemRemoved(QnWorkbenchItem*)), m_resourcesTreeView, SLOT(clearSelection())); // ### update(QModelIndex)
}

Q_DECLARE_METATYPE(ResourceSortFilterProxyModel *) // ###

void NavigationTreeWidget::workbenchLayoutChanged()
{
    if (!m_controller)
        return;

    connect(m_controller->layout(), SIGNAL(itemAdded(QnWorkbenchItem*)), m_resourcesTreeView, SLOT(clearSelection())); // ### update(QModelIndex)
    connect(m_controller->layout(), SIGNAL(itemRemoved(QnWorkbenchItem*)), m_resourcesTreeView, SLOT(clearSelection())); // ### update(QModelIndex)

    m_searchProxyModel = m_controller->layout()->property("model").value<ResourceSortFilterProxyModel *>(); // ###
    if (!m_searchProxyModel) {
        m_controller->layout()->setProperty("model", QVariant::fromValue(new ResourceSortFilterProxyModel(m_controller->workbench()))); // ###
        m_searchProxyModel = m_controller->layout()->property("model").value<ResourceSortFilterProxyModel *>(); // ###
        m_searchProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_searchProxyModel->setFilterKeyColumn(0);
        m_searchProxyModel->setFilterRole(Qt::UserRole + 2);
        m_searchProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_searchProxyModel->setDynamicSortFilter(true);
        m_searchProxyModel->setSourceModel(m_resourcesModel);
    }

    connect(m_searchProxyModel.data(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleInsertRows(QModelIndex,int,int)));
    connect(m_searchProxyModel.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), this, SLOT(handleRemoveRows(QModelIndex,int,int)));

    m_searchTreeView->setModel(m_searchProxyModel);
    m_searchTreeView->expandAll();

    disconnect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    m_filterLineEdit->setText(m_searchProxyModel->filterRegExp().pattern()); // ###
    m_clearFilterButton->setVisible(!m_filterLineEdit->text().isEmpty());
    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
}

void NavigationTreeWidget::handleInsertRows(const QModelIndex &parent, int first, int last)
{
    if (!m_controller || !m_searchProxyModel)
        return;

    for (int row = first; row <= last; ++row) {
        const QModelIndex index = m_searchProxyModel->index(row, 0, parent);
        QnResourcePtr resource = qnResPool->getResourceById(index.data(Qt::UserRole + 1));
        m_controller->drop(resource);
    }
}

void NavigationTreeWidget::handleRemoveRows(const QModelIndex &parent, int first, int last)
{
    if (!m_controller || !m_searchProxyModel)
        return;

    for (int row = first; row <= last; ++row) {
        const QModelIndex index = m_searchProxyModel->index(row, 0, parent);
        QnResourcePtr resource = qnResPool->getResourceById(index.data(Qt::UserRole + 1));
        m_controller->layout()->removeItem(m_controller->item(resource));
    }
}

void NavigationTreeWidget::contextMenuEvent(QContextMenuEvent *)
{
    QnResourceList resources;
    QAbstractItemView *view = m_tabWidget->currentIndex() == 0 ? m_resourcesTreeView : m_searchTreeView;
    foreach (const QModelIndex &index, view->selectionModel()->selectedRows())
        resources.append(qnResPool->getResourceById(index.data(Qt::UserRole + 1)));

    QScopedPointer<QMenu> menu(new QMenu);

    QAction *openAction = new QAction(tr("Open"), menu.data());
    connect(openAction, SIGNAL(triggered()), this, SLOT(open()));
    menu->addAction(openAction);
    menu->addSeparator();
    if (resources.size() == 1) {
        const QnResourcePtr resource = resources.first();
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

        if (CLDeviceSettingsDlgFactory::canCreateDlg(resource))
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
        const QnResourcePtr resource = resources.first();
        if (action == &cm_settings) { // ### move to app-global scope
            if (QDialog *dialog = CLDeviceSettingsDlgFactory::createDlg(resource, QApplication::activeWindow())) {
                dialog->exec();
                delete dialog;
            }
        } else if (action == &cm_editTags) { // ### move to app-global scope
            TagsEditDialog dialog(QStringList() << resource->getUniqueId(), this);
            dialog.setWindowModality(Qt::ApplicationModal);
            dialog.exec();
        } else if (action == &cm_upload_youtube) { // ### move to app-global scope
            YouTubeUploadDialog dialog(resource, this);
            dialog.setWindowModality(Qt::ApplicationModal);
            dialog.exec();
        }
    }
}

void NavigationTreeWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_filterTimerId) {
        killTimer(m_filterTimerId);
        m_filterTimerId = 0;

        if (m_searchProxyModel) {
            m_searchProxyModel->setFilterWildcard(m_filterLineEdit->text());
            m_searchTreeView->expandAll();
        }
    }

    QWidget::timerEvent(event);
}

void NavigationTreeWidget::filterChanged(const QString &filter)
{
    m_clearFilterButton->setVisible(!filter.isEmpty());

    if (m_filterTimerId != 0)
        killTimer(m_filterTimerId);
    m_filterTimerId = startTimer(!filter.isEmpty() ? qMax(1000 - filter.size() * 100, 50) : 0);
}

void NavigationTreeWidget::itemActivated(const QModelIndex &index)
{
    Q_EMIT activated(index.data(Qt::UserRole + 1).toUInt());
}

void NavigationTreeWidget::open()
{
    QAbstractItemView *view = m_tabWidget->currentIndex() == 0 ? m_resourcesTreeView : m_searchTreeView;
    foreach (const QModelIndex &index, view->selectionModel()->selectedRows())
        itemActivated(index);
}

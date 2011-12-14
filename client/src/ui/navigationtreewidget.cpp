#include "navigationtreewidget.h"

#include <QtGui/QAction>
#include <QtGui/QBoxLayout>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QLineEdit>
#include <QtGui/QMenu>
#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/context_menu_helper.h"
#include "ui/device_settings/dlg_factory.h"
#include "ui/models/resourcemodel.h"
#include "ui/skin.h"

class NavigationTreeSortFilterProxyModel : public QSortFilterProxyModel
{
public:
    NavigationTreeSortFilterProxyModel(QObject *parent = 0)
        : QSortFilterProxyModel(parent)
    {}

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
    {
        return !source_parent.isValid() || QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }
};


NavigationTreeWidget::NavigationTreeWidget(QWidget *parent)
    : QWidget(parent)
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
    m_clearFilterButton->setIcon(Skin::icon(QLatin1String("close2.png")));
    m_clearFilterButton->setIconSize(QSize(16, 16));

    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(m_clearFilterButton, SIGNAL(clicked()), m_filterLineEdit, SLOT(clear()));


    m_navigationTreeView = new QTreeView(this);

    m_model = new ResourceModel(m_navigationTreeView);

    m_proxyModel = new NavigationTreeSortFilterProxyModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(0);
    m_proxyModel->setFilterRole(Qt::UserRole + 2);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_model);

    m_navigationTreeView->setModel(m_proxyModel);
    m_navigationTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_navigationTreeView->setAllColumnsShowFocus(true);
    m_navigationTreeView->setRootIsDecorated(true);
    m_navigationTreeView->setHeaderHidden(true); // ###
    m_navigationTreeView->setSortingEnabled(false); // ###
    m_navigationTreeView->setUniformRowHeights(true);
    m_navigationTreeView->setWordWrap(false);

    connect(m_navigationTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(itemActivated(QModelIndex)));


    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(3);
    topLayout->addWidget(m_previousItemButton);
    topLayout->addWidget(m_nextItemButton);
    topLayout->addWidget(m_filterLineEdit);
    topLayout->addWidget(m_clearFilterButton);
/*
    QWidget *searchTab = new QWidget(this);

    QVBoxLayout *searchTabLayout = new QVBoxLayout;
    searchTabLayout->setContentsMargins(0, 0, 0, 0);
    searchTabLayout->addLayout(topLayout);
    //searchTabLayout->addWidget(m_navigationTreeView);
    searchTab->setLayout(searchTabLayout);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_navigationTreeView, tr("Resources"));
    m_tabWidget->addTab(searchTab, tr("Search"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
*/
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_navigationTreeView);
    setLayout(mainLayout);

    setMinimumWidth(200);
    setMaximumWidth(350);
    setAcceptDrops(true);
}

NavigationTreeWidget::~NavigationTreeWidget()
{
}

void NavigationTreeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QnResourceList resources;
    foreach (const QModelIndex &index, m_navigationTreeView->selectionModel()->selectedRows())
        resources.append(qnResPool->getResourceById(QnId(QString::number(index.data(Qt::UserRole + 1).toUInt()))));

    QMenu menu;

    if (resources.size() == 1)
    {
        const QnResourcePtr resource = resources.first();
        if (resource->checkFlag(QnResource::video) || resource->checkFlag(QnResource::SINGLE_SHOT))
        {
            menu.addAction(&cm_editTags);

            if (resource->associatedWithFile())
                menu.addAction(&cm_remove_from_disk);

            if (resource->checkFlag(QnResource::ARCHIVE))
                menu.addAction(&cm_upload_youtube);

            if (resource->checkFlag(QnResource::ARCHIVE) || resource->checkFlag(QnResource::SINGLE_SHOT))
                menu.addAction(&cm_open_containing_folder);
        }
        else if (resource->checkFlag(QnResource::live_cam))
        {
            // ### Start/Stop recording (Ctrl+R)
            // ### Delete(if no connection)(Shift+Del)
            menu.addAction(&cm_editTags);
            // ### Export selected... (Ctrl+E)
        }
        else if (resource->checkFlag(QnResource::server))
        {
            // ### New camera
        }
        // ### handle layouts

        if (CLDeviceSettingsDlgFactory::canCreateDlg(resource))
            menu.addAction(&cm_settings);
    }
    else if (resources.size() > 1)
    {
        // ###
    }
    else
    {
        menu.addAction(&cm_open_file);
        menu.addAction(&cm_new_item);
    }
    menu.addSeparator();
    menu.addAction(&cm_toggle_fullscreen);
    menu.addAction(&cm_screen_recording);
    menu.addAction(&cm_preferences);
    menu.addSeparator();
    menu.addAction(&cm_exit);

    /*QAction *action = */menu.exec(event->globalPos());
}

void NavigationTreeWidget::filterChanged(const QString &filter)
{
    if (!filter.isEmpty())
    {
        m_clearFilterButton->show();
        m_proxyModel->setFilterWildcard(QLatin1Char('*') + filter + QLatin1Char('*'));
    }
    else
    {
        m_clearFilterButton->hide();
        m_proxyModel->invalidate();
    }
}

void NavigationTreeWidget::itemActivated(const QModelIndex &index)
{
    Q_EMIT activated(index.data(Qt::UserRole + 1).toUInt());
}

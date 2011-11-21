#include "navigationtreewidget.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>

#include "ui/skin.h"
#include "ui/models/resourcemodel.h"

NavigationTreeWidget::NavigationTreeWidget(QWidget *parent)
    : QWidget(parent)
{
    m_filterLineEdit = new QLineEdit(this);
    m_filterLineEdit->setMaxLength(150);

    m_clearFilterButton = new QToolButton(this);
    m_clearFilterButton->setText(QLatin1String("X"));
    m_clearFilterButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_clearFilterButton->setIcon(Skin::icon(QLatin1String("close2.png")));

    connect(m_filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(m_clearFilterButton, SIGNAL(clicked()), m_filterLineEdit, SLOT(clear()));


    m_navigationTreeView = new QTreeView(this);

    m_model = new ResourceModel(m_navigationTreeView);

    m_proxyModel = new QSortFilterProxyModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setSourceModel(m_model);

    m_navigationTreeView->setModel(m_proxyModel);
    m_navigationTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_navigationTreeView->setAllColumnsShowFocus(true);
    m_navigationTreeView->setRootIsDecorated(true);
    m_navigationTreeView->setHeaderHidden(true);
    m_navigationTreeView->setSortingEnabled(false);
    m_navigationTreeView->setUniformRowHeights(true);
    m_navigationTreeView->setWordWrap(false);

    connect(m_navigationTreeView, SIGNAL(activated(QModelIndex)), this, SLOT(itemActivated(QModelIndex)));


    QHBoxLayout *filterLayout = new QHBoxLayout;
    filterLayout->setSpacing(3);
    filterLayout->addWidget(m_filterLineEdit);
    filterLayout->addWidget(m_clearFilterButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(m_navigationTreeView);
    setLayout(mainLayout);
}

NavigationTreeWidget::~NavigationTreeWidget()
{
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
        m_proxyModel->setFilterFixedString(filter);
    }
}

void NavigationTreeWidget::itemActivated(const QModelIndex &index)
{
    bool ok;
    const uint resourceId = index.data(Qt::UserRole + 1).toUInt(&ok);
    if (ok)
        Q_EMIT activated(resourceId);
}

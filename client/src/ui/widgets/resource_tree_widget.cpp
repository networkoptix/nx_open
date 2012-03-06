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
#include <utils/common/scoped_painter_rollback.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/video_server.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/models/resource_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>

#include "ui_resource_tree_widget.h"

Q_DECLARE_METATYPE(QnResourceSearchProxyModel *);
Q_DECLARE_METATYPE(QnResourceSearchSynchronizer *);

namespace {
    const char *qn_searchModelPropertyName = "_qn_searchModel";
    const char *qn_searchSynchronizerPropertyName = "_qn_searchSynchronizer";
    const char *qn_searchStringPropertyName = "_qn_searchString";
    const char *qn_searchCriterionPropertyName = "_qn_searchCriterion";
}

class QnResourceTreeItemDelegate: public QStyledItemDelegate {
    typedef QStyledItemDelegate base_type;

public:
    explicit QnResourceTreeItemDelegate(QObject *parent = NULL): 
        base_type(parent)
    {}

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

        QStyle *style = optionV4.widget ? optionV4.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter, optionV4.widget);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource && resource->getStatus() == QnResource::Offline) {
            QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 0, 0, 128), 0));

            QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &optionV4, optionV4.widget);
            painter->drawLine(iconRect.topLeft() + QPoint(1, 1), iconRect.bottomRight() - QPoint(1, 1));
            painter->drawLine(iconRect.topRight() + QPoint(-1, 1), iconRect.bottomLeft() - QPoint(-1, 1));
        }
    }

    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        base_type::initStyleOption(option, index);

        if(workbench() == NULL)
            return;

        QnResourcePtr currentLayoutResource = workbench()->currentLayout()->resource();
        if(!currentLayoutResource)
            return;

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource.isNull())
            return;
        QnResourcePtr parentResource = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
        QUuid uuid = index.data(Qn::UuidRole).value<QUuid>();

        bool bold = false;
        if(resource == currentLayoutResource) {
            bold = true; /* Bold current layout. */
        } else if(parentResource == currentLayoutResource) {
            bold = true; /* Bold items of the current layout. */
        } else if(uuid.isNull() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()) {
            bold = true; /* Bold items of the current layout in servers. */
        }

        option->font.setBold(bold);
    }

private:
    QWeakPointer<QnWorkbench> m_workbench;
};


QnResourceTreeWidget::QnResourceTreeWidget(QWidget *parent): 
    QWidget(parent),
    ui(new Ui::ResourceTreeWidget()),
    m_context(NULL),
    m_filterTimerId(0),
    m_ignoreFilterChanges(false)
{
    ui->setupUi(this);

    ui->typeComboBox->addItem(tr("Any Type"), 0);
    ui->typeComboBox->addItem(tr("Video Files"), static_cast<int>(QnResource::local | QnResource::video));
    ui->typeComboBox->addItem(tr("Image Files"), static_cast<int>(QnResource::still_image));
    ui->typeComboBox->addItem(tr("Live Cameras"), static_cast<int>(QnResource::live));

    ui->clearFilterButton->setIcon(Skin::icon(QLatin1String("clear.png")));
    ui->clearFilterButton->setIconSize(QSize(16, 16));

    m_resourceModel = new QnResourceModel(this);
    ui->resourceTreeView->setModel(m_resourceModel);

    m_resourceDelegate = new QnResourceTreeItemDelegate(this);
    ui->resourceTreeView->setItemDelegate(m_resourceDelegate);

    m_searchDelegate = new QnResourceTreeItemDelegate(this);
    ui->searchTreeView->setItemDelegate(m_searchDelegate);

    m_renameLayoutAction = new QAction(this);

    connect(ui->typeComboBox,       SIGNAL(currentIndexChanged(int)),   this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(textChanged(QString)),       this,               SLOT(updateFilter()));
    connect(ui->filterLineEdit,     SIGNAL(editingFinished()),          this,               SLOT(forceUpdateFilter()));
    connect(ui->clearFilterButton,  SIGNAL(clicked()),                  ui->filterLineEdit, SLOT(clear()));
    connect(ui->resourceTreeView,   SIGNAL(activated(QModelIndex)),     this,               SLOT(at_treeView_activated(QModelIndex)));
    connect(ui->searchTreeView,     SIGNAL(activated(QModelIndex)),     this,               SLOT(at_treeView_activated(QModelIndex)));
    connect(ui->tabWidget,          SIGNAL(currentChanged(int)),        this,               SLOT(at_tabWidget_currentChanged(int)));

    updateFilter();
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    return;
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

QnWorkbench *QnResourceTreeWidget::workbench() const {
    return m_context ? m_context->workbench() : NULL;
}

void QnResourceTreeWidget::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL) {
        disconnect(m_context, NULL, this, NULL);
        disconnect(workbench(), NULL, this, NULL);

        at_workbench_currentLayoutAboutToBeChanged();

        ui->filterLineEdit->setEnabled(false);
        ui->typeComboBox->setEnabled(false);
        m_searchDelegate->setWorkbench(NULL);
        m_resourceDelegate->setWorkbench(NULL);

        m_resourceModel->setContext(NULL);
    }

    m_context = context;

    if(m_context) {
        m_resourceModel->setContext(context);

        m_searchDelegate->setWorkbench(workbench());
        m_resourceDelegate->setWorkbench(workbench());
        ui->filterLineEdit->setEnabled(true);
        ui->typeComboBox->setEnabled(true);

        at_workbench_currentLayoutChanged();

        connect(m_context,          SIGNAL(aboutToBeDestroyed()),                       this, SLOT(at_context_aboutToBeDestroyed()));
        connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),            this, SLOT(at_workbench_currentLayoutAboutToBeChanged()));
        connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this, SLOT(at_workbench_currentLayoutChanged()));
        connect(workbench(),        SIGNAL(itemAdded(QnWorkbenchItem *)),               this, SLOT(at_workbench_itemAdded(QnWorkbenchItem *)));
        connect(workbench(),        SIGNAL(itemRemoved(QnWorkbenchItem *)),             this, SLOT(at_workbench_itemRemoved(QnWorkbenchItem *)));
    }
}

void QnResourceTreeWidget::open() {
    QAbstractItemView *view = ui->tabWidget->currentIndex() == 0 ? ui->resourceTreeView : ui->searchTreeView;
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

        /* Always accept servers. */
        result->addCriterion(QnResourceCriterion(QnResource::server)); 
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

void QnResourceTreeWidget::killSearchTimer() {
    if (m_filterTimerId == 0) 
        return;

    killTimer(m_filterTimerId);
    m_filterTimerId = 0; 
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

    foreach (const QModelIndex &index, currentSelectionModel()->selectedRows())
        result.append(index.data(Qn::ResourceRole).value<QnResourcePtr>());

    return result;
}

QnLayoutItemIndexList QnResourceTreeWidget::selectedLayoutItems() const {
    QnLayoutItemIndexList result;

    foreach (const QModelIndex &index, currentSelectionModel()->selectedRows()) {
        QUuid uuid = index.data(Qn::UuidRole).value<QUuid>();
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

    if(!selectionModel->currentIndex().data(Qn::UuidRole).value<QUuid>().isNull()) { /* If it's a layout item. */
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
        int pos = qMax(filter.lastIndexOf(QChar('+')), filter.lastIndexOf(QChar('\\'))) + 1;
        
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceTreeWidget::contextMenuEvent(QContextMenuEvent *) {
    if(!context() || !context()->menu()) {
        qnWarning("Requesting context menu for a tree widget while no menu manager instance is available.");
        return;
    }
    QnActionManager *manager = context()->menu();

    QScopedPointer<QMenu> menu(manager->newMenu(Qn::TreeScope, currentTarget(Qn::TreeScope)));
    if(menu->isEmpty())
        return;

    /* Add tree-local actions to the menu. */
    manager->redirectAction(menu.data(), Qn::RenameLayoutAction, m_renameLayoutAction);

    /* Run menu. */
    QAction *action = menu->exec(QCursor::pos());

    /* Process tree-local actions. */
    if(action == m_renameLayoutAction)
        currentItemView()->edit(currentSelectionModel()->currentIndex());
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

        if (workbench()) {
            QnWorkbenchLayout *layout = workbench()->currentLayout();
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

    QnResourceSearchSynchronizer *synchronizer = layoutSynchronizer(layout, false);
    if(synchronizer)
        synchronizer->disableUpdates();
    setLayoutSearchString(layout, ui->filterLineEdit->text());

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
    ui->filterLineEdit->setText(layoutSearchString(layout));

    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceTreeWidget::at_workbench_itemAdded(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceTreeWidget::at_workbench_itemRemoved(QnWorkbenchItem *) {
    /* Bold state has changed. */
    currentItemView()->update();
}

void QnResourceTreeWidget::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnResourceTreeWidget::at_tabWidget_currentChanged(int index) {
    if(index == SearchTab) {
        QnWorkbenchLayout *layout = workbench()->currentLayout();

        layoutSynchronizer(layout, true); /* Just initialize the synchronizer. */
        QnResourceSearchProxyModel *model = layoutModel(layout, true);

        ui->searchTreeView->setModel(model);
        ui->searchTreeView->expandAll();
    }

    emit currentTabChanged();
}

void QnResourceTreeWidget::at_treeView_activated(const QModelIndex &index) {
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
        emit activated(resource);
}



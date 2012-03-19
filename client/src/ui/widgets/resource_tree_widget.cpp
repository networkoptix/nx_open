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
#include <ui/models/resource_pool_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>

#include "ui_resource_tree_widget.h"
#include "ui/style/proxy_style.h"

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
    {
        m_recIcon = qnSkin->icon("decorations/recording.png");
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

        QStyle *style = optionV4.widget ? optionV4.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter, optionV4.widget);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

        if(resource && resource->getStatus() == QnResource::Recording) {
            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4, optionV4.widget);
            int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, optionV4.widget) + 1;
            textRect = textRect.adjusted(textMargin, 0, -textMargin, 0);

            QFontMetrics metrics(optionV4.font);
            int textWidth = metrics.width(optionV4.text);

            QRect iconRect = QRect(
                textRect.left() + textWidth + textMargin,
                textRect.top() + 2,
                textRect.height() - 4,
                textRect.height() - 4
            );

            m_recIcon.paint(painter, iconRect);
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
    QIcon m_recIcon;
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
    QnWorkbenchContextAware(parent ? static_cast<QObject *>(parent) : context),
    ui(new Ui::ResourceTreeWidget()),
    m_filterTimerId(0),
    m_ignoreFilterChanges(false)
{
    ui->setupUi(this);

    ui->typeComboBox->addItem(tr("Any Type"), 0);
    ui->typeComboBox->addItem(tr("Video Files"), static_cast<int>(QnResource::local | QnResource::video));
    ui->typeComboBox->addItem(tr("Image Files"), static_cast<int>(QnResource::still_image));
    ui->typeComboBox->addItem(tr("Live Cameras"), static_cast<int>(QnResource::live));

    ui->clearFilterButton->setIcon(qnSkin->icon("clear.png"));
    ui->clearFilterButton->setIconSize(QSize(16, 16));

    m_resourceModel = new QnResourcePoolModel(this);
    QSortFilterProxyModel *model = new QnResourceTreeSortProxyModel(this);
    model->setSourceModel(m_resourceModel);
    model->setSupportedDragActions(m_resourceModel->supportedDragActions());
    model->setDynamicSortFilter(true);
    model->setSortRole(Qt::DisplayRole);
    model->setSortCaseSensitivity(Qt::CaseInsensitive);
    model->sort(0);
    ui->resourceTreeView->setModel(model);

    m_resourceDelegate = new QnResourceTreeItemDelegate(this);
    ui->resourceTreeView->setItemDelegate(m_resourceDelegate);

    m_searchDelegate = new QnResourceTreeItemDelegate(this);
    ui->searchTreeView->setItemDelegate(m_searchDelegate);

    QnResourceTreeStyle *treeStyle = new QnResourceTreeStyle(style(), this);
    ui->resourceTreeView->setStyle(treeStyle);
    ui->searchTreeView->setStyle(treeStyle);

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

    updateFilter();

    /* Connect to context. */
    m_searchDelegate->setWorkbench(workbench());
    m_resourceDelegate->setWorkbench(workbench());
    ui->filterLineEdit->setEnabled(true);
    ui->typeComboBox->setEnabled(true);

    at_workbench_currentLayoutChanged();

    connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),            this, SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this, SLOT(at_workbench_currentLayoutChanged()));
    connect(workbench(),        SIGNAL(itemAdded(QnWorkbenchItem *)),               this, SLOT(at_workbench_itemAdded(QnWorkbenchItem *)));
    connect(workbench(),        SIGNAL(itemRemoved(QnWorkbenchItem *)),             this, SLOT(at_workbench_itemRemoved(QnWorkbenchItem *)));
}

QnResourceTreeWidget::~QnResourceTreeWidget() {
    disconnect(workbench(), NULL, this, NULL);

    at_workbench_currentLayoutAboutToBeChanged();

    ui->filterLineEdit->setEnabled(false);
    ui->typeComboBox->setEnabled(false);
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

void QnResourceTreeWidget::open() {
    QAbstractItemView *view = ui->tabWidget->currentIndex() == 0 ? ui->resourceTreeView : ui->searchTreeView;
    foreach (const QModelIndex &index, view->selectionModel()->selectedRows())
        at_treeView_doubleClicked(index);
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

    /* Add tree-local actions to the menu. */
    manager->redirectAction(menu.data(), Qn::RenameAction, m_renameAction);
    if(currentSelectionModel()->currentIndex().data(Qn::NodeTypeRole) != Qn::UsersNode || !currentSelectionModel()->selection().contains(currentSelectionModel()->currentIndex()))
        manager->redirectAction(menu.data(), Qn::NewUserAction, NULL); /* Show 'New User' item only when clicking on 'Users' node. */

    if(menu->isEmpty())
        return;

    /* Run menu. */
    QAction *action = menu->exec(QCursor::pos());

    /* Process tree-local actions. */
    if(action == m_renameAction)
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
    if (resource && !(resource->flags() & QnResource::layout)) /* Layouts cannot be activated by double clicking. */
        emit activated(resource);
}


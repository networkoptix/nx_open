#include "accessible_resources_widget.h"
#include "accessible_resources_model_p.h"
#include "ui_accessible_resources_widget.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>

#include <client/client_globals.h>
#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/delegates/customizable_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/event_processors.h>

#include <nx/utils/string.h>
#include <nx/utils/app_info.h>

namespace {

const QString kDummyResourceId(lit("dummy_resource"));

} // anonymous namespace


QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(
    QnAbstractPermissionsModel* permissionsModel,
    QnResourceAccessFilter::Filter filter,
    QWidget* parent /*= 0*/)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_permissionsModel(permissionsModel),
    m_filter(filter),
    m_controlsVisible(filter == QnResourceAccessFilter::MediaFilter), /*< Show 'All' checkbox only for cameras. */
    m_resourcesModel(new QnResourceListModel(this)),
    m_controlsModel(new QnResourceListModel(this)),
    m_sortFilterModel(new QnResourceListSortedModel(this)),
    m_accessibleResourcesModel(new QnAccessibleResourcesModel(this))
{
    ui->setupUi(this);
    switch (m_filter)
    {
        case QnResourceAccessFilter::LayoutsFilter:
            ui->detailsWidget->setDescription(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on these layouts."));
            break;
        default:
            break;
    }

    initControlsModel();
    initResourcesModel();
    initSortFilterModel();

    m_accessibleResourcesModel->setSourceModel(m_sortFilterModel);

    connect(this, &QnAccessibleResourcesWidget::controlsChanged,
        m_accessibleResourcesModel, &QnAccessibleResourcesModel::setAllChecked);

    ui->resourcesTreeView->setModel(m_accessibleResourcesModel);
    ui->controlsTreeView->setModel(m_controlsModel);
    ui->controlsTreeView->setVisible(m_controlsVisible);
    ui->controlsTreeView->setEnabled(m_controlsVisible);
    ui->line->setVisible(m_controlsVisible);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->resourcesListWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    if (m_controlsVisible)
    {
        installEventHandler({ ui->resourcesTreeView, ui->controlsTreeView }, QEvent::KeyPress,
            this, &QnAccessibleResourcesWidget::at_itemViewKeyPress);
    }

    installEventHandler(scrollBar, { QEvent::Show, QEvent::Hide }, this,
        [this, scrollBar](QObject* object, QEvent* event)
        {
            Q_UNUSED(object);
            QMargins margins = ui->resourceListLayout->contentsMargins();
            int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
            margins.setRight(event->type() == QEvent::Show ? margin + scrollBar->width() : margin);
            ui->resourceListLayout->setContentsMargins(margins);
            ui->resourceListLayout->activate();
        });

    auto itemDelegate = new QnResourceItemDelegate(this);
    itemDelegate->setCustomInfoLevel(Qn::RI_FullInfo);

    auto setupTreeView = [itemDelegate](QnTreeView* treeView)
        {
            const QnIndents kIndents(1, 0);
            treeView->setItemDelegateForColumn(QnAccessibleResourcesModel::NameColumn,
                itemDelegate);
            treeView->header()->setMinimumSectionSize(0);
            treeView->header()->setStretchLastSection(false);
            treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
            treeView->header()->setSectionResizeMode(QnAccessibleResourcesModel::NameColumn,
                QHeaderView::Stretch);
            treeView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(kIndents));
            treeView->setIgnoreDefaultSpace(true);
        };
    setupTreeView(ui->resourcesTreeView);
    setupTreeView(ui->controlsTreeView);

    auto indirectAccessDelegate = new QnCustomizableItemDelegate(this);
    indirectAccessDelegate->setCustomSizeHint(
        [](const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            return qnSkin->maximumSize(option.icon);
        });

    if (nx::utils::AppInfo::isLinux() || nx::utils::AppInfo::isMacOsX())
    {
        /**
          * Workaround for incorrect selection behaviour on MacOS. For some reason QTreeview
          * assumes that column 0 and 2 are selected, but column 1 is not.
          */
        indirectAccessDelegate->setCustomInitStyleOption(
            [this](QStyleOptionViewItem* option, const QModelIndex& index)
            {
                const auto selectionModel = ui->resourcesTreeView->selectionModel();
                const auto nameIndex = index.sibling(index.row(),
                    QnAccessibleResourcesModel::NameColumn);
                if (selectionModel->isSelected(nameIndex))
                    option->state |= QStyle::State_Selected;
            });
    }

    indirectAccessDelegate->setCustomPaint(
        [](QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem,
                &option, painter, option.widget);
            option.icon.paint(painter, option.rect, Qt::AlignCenter,
                option.state.testFlag(QStyle::State_Selected)
                    ? QIcon::Normal
                    : QIcon::Disabled);
        });

    ui->resourcesTreeView->setItemDelegateForColumn(
        QnAccessibleResourcesModel::IndirectAccessColumn,
        indirectAccessDelegate);

    ui->resourcesTreeView->setMouseTracking(true);

    auto toggleCheckbox = [this](const QModelIndex& index)
        {
            auto tree = static_cast<QnTreeView*>(sender());
            bool controlsView = tree == ui->controlsTreeView;

            if (!controlsView && qApp->keyboardModifiers())
                return;

            QAbstractItemModel* model = tree->model();
            int column = (controlsView)
                ? static_cast<int>(QnResourceListModel::CheckColumn)
                : static_cast<int>(QnAccessibleResourcesModel::CheckColumn);
            QModelIndex checkboxIdx = index.sibling(index.row(), column);
            int newCheckValue = checkboxIdx.data(Qt::CheckStateRole).toInt() != Qt::Checked ? Qt::Checked : Qt::Unchecked;
            model->setData(checkboxIdx, newCheckValue, Qt::CheckStateRole);
        };

    connect(ui->resourcesTreeView, &QnTreeView::clicked, this, toggleCheckbox);
    connect(ui->controlsTreeView, &QnTreeView::clicked, this, toggleCheckbox);

    auto batchToggleCheckboxes = [this](const QModelIndex& index)
        {
            Q_UNUSED(index);
            QnTreeView* tree = static_cast<QnTreeView*>(sender());
            QAbstractItemModel* model = tree->model();
            int column = (tree == ui->controlsTreeView)
                ? static_cast<int>(QnResourceListModel::CheckColumn)
                : static_cast<int>(QnAccessibleResourcesModel::CheckColumn);
            QModelIndexList selectedRows = tree->selectionModel()->selectedRows(column);

            /* If any of selected rows were unchecked check all, otherwise uncheck all: */
            bool wasUnchecked = boost::algorithm::any_of(selectedRows, [this](const QModelIndex& index)
            {
                return index.data(Qt::CheckStateRole).toInt() != Qt::Checked;
            });

            int newCheckValue = wasUnchecked ? Qt::Checked : Qt::Unchecked;

            for (QModelIndex index : selectedRows)
                model->setData(index, newCheckValue, Qt::CheckStateRole);
        };

    connect(ui->resourcesTreeView, &QnTreeView::spacePressed, this, batchToggleCheckboxes);
    connect(ui->controlsTreeView,  &QnTreeView::spacePressed, this, batchToggleCheckboxes);

    connect(ui->resourcesTreeView, &QnTreeView::selectionChanging,
        this, &QnAccessibleResourcesWidget::handleSelectionChanging);

    connect(ui->resourcesTreeView, &QAbstractItemView::entered, this, &QnAccessibleResourcesWidget::updateThumbnail);
    updateThumbnail();
}

QnAccessibleResourcesWidget::~QnAccessibleResourcesWidget()
{
}

bool QnAccessibleResourcesWidget::hasChanges() const
{
    if (m_controlsVisible)
    {
        bool checkedAll = !m_controlsModel->checkedResources().isEmpty();
        if (m_permissionsModel->rawPermissions().testFlag(Qn::GlobalAccessAllMediaPermission) != checkedAll)
            return true;
    }

    return m_resourcesModel->checkedResources() != QnResourceAccessFilter::filteredResources(
        resourcePool(),
        m_filter,
        m_permissionsModel->accessibleResources());
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    refreshModel();

    if (m_controlsVisible)
    {
        bool hasAllMedia = m_permissionsModel->rawPermissions().testFlag(
            Qn::GlobalAccessAllMediaPermission);

        /* For custom users 'All Resources' must be unchecked by default */
        if (m_permissionsModel->subject().user())
        {
            hasAllMedia &= m_permissionsModel->rawPermissions().testFlag(
                Qn::GlobalCustomUserPermission);
        }

        QSet<QnUuid> checkedControls;
        if (hasAllMedia)
        {
            /* Really we are checking the only dummy resource. */
            for (const auto& resource : m_controlsModel->resources())
                checkedControls << resource->getId();
        }
        m_controlsModel->setCheckedResources(checkedControls);
    }

    m_resourcesModel->setCheckedResources(QnResourceAccessFilter::filteredResources(
        resourcePool(),
        m_filter,
        m_permissionsModel->accessibleResources()));
}

bool QnAccessibleResourcesWidget::isAll() const
{
    return m_controlsVisible && !m_controlsModel->checkedResources().isEmpty();
}

std::pair<int, int> QnAccessibleResourcesWidget::selected() const
{
    return std::make_pair(m_resourcesModel->checkedResources().size(), m_resourcesModel->rowCount());
}

QnResourceAccessFilter::Filter QnAccessibleResourcesWidget::filter() const
{
    return m_filter;
}

QSet<QnUuid> QnAccessibleResourcesWidget::checkedResources() const
{
    return m_resourcesModel->checkedResources();
}

void QnAccessibleResourcesWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnResourceAccessFilter::filteredResources(
        resourcePool(),
        m_filter,
        accessibleResources);
    auto newFiltered = m_resourcesModel->checkedResources();

    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);

    /* Some resources may be deleted while we are editing user. Do not store them in DB. */
    QSet<QnUuid> unavailable;
    for (const QnUuid& id : accessibleResources)
    {
        if (!resourcePool()->getResourceById(id))
            unavailable << id;
    }

    accessibleResources.subtract(unavailable);

    m_permissionsModel->setAccessibleResources(accessibleResources);

    if (m_controlsVisible)
    {
        bool checkedAll = !m_controlsModel->checkedResources().isEmpty();
        Qn::GlobalPermissions permissions = m_permissionsModel->rawPermissions();
        if (checkedAll)
            permissions |= Qn::GlobalAccessAllMediaPermission;
        else
            permissions &= ~Qn::GlobalAccessAllMediaPermission;
        m_permissionsModel->setRawPermissions(permissions);
    }
}

void QnAccessibleResourcesWidget::initControlsModel()
{
    if (!m_controlsVisible)
        return;

    QnVirtualCameraResourcePtr dummy(new QnClientCameraResource(qnResTypePool->getFixedResourceTypeId(kDummyResourceId)));
    dummy->setName(tr("All Cameras & Resources"));
    /* Create separate dummy resource id for each filter, but once per application run. */
    dummy->setId(QnUuid::createUuidFromPool(guidFromArbitraryData(kDummyResourceId).getQUuid(), m_filter));
    qnResIconCache->setKey(dummy, QnResourceIconCache::Cameras);
    m_controlsModel->setResources(QnResourceList({ dummy }));
    m_controlsModel->setHasCheckboxes(true);
    m_controlsModel->setUserCheckable(false);
    m_controlsModel->setSimplified(true);

    auto modelUpdated = [this](const QModelIndex& index = QModelIndex())
    {
        QModelIndex checkedIdx = index.isValid() ?
            index.sibling(index.row(), QnResourceListModel::CheckColumn) :
            m_controlsModel->index(0, QnResourceListModel::CheckColumn);

        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        ui->resourcesTreeView->selectionModel()->reset();
        ui->resourcesTreeView->setEnabled(!checked);
        ui->filter->setEnabled(!checked);
        emit controlsChanged(checked);
        emit hasChangesChanged();
    };

    connect(m_controlsModel, &QnResourceListModel::dataChanged, this, modelUpdated);
    connect(m_controlsModel, &QnResourceListModel::modelReset,  this, modelUpdated);
}

bool QnAccessibleResourcesWidget::resourcePassFilter(const QnResourcePtr& resource) const
{
    return resourcePassFilter(resource, context()->user(), m_filter);
}

bool QnAccessibleResourcesWidget::resourcePassFilter(const QnResourcePtr& resource,
    const QnUserResourcePtr& currentUser, QnResourceAccessFilter::Filter filter)
{
    if (!currentUser)
        return false;

    if (!QnResourceAccessFilter::isShareable(filter, resource))
        return false;

    /* Additionally filter layouts. */
    if (filter == QnResourceAccessFilter::LayoutsFilter)
    {
        QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);
        if (!layout || !layout->resourcePool())
            return false;

        /* Hide "Preview Search" layouts */
        if (layout->data().contains(Qn::LayoutSearchStateRole)) //TODO: #GDM make it consistent with QnWorkbenchLayout::isSearchLayout
            return false;

        /* Hide autogenerated layouts, e.g from videowall. */
        if (layout->isServiceLayout())
            return false;

        /* Hide unsaved layouts. */
        if (layout->hasFlags(Qn::local))
            return false;

        /* Show only shared or belonging to current user. */
        return layout->isShared() || layout->getParentId() == currentUser->getId();
    }
    else
    {
        return true;
    }
};

void QnAccessibleResourcesWidget::initResourcesModel()
{
    m_resourcesModel->setHasCheckboxes(true);
    m_resourcesModel->setUserCheckable(false);
    m_resourcesModel->setSimplified(true);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnAccessibleResourcesWidget::handleResourceAdded);

    if (m_filter == QnResourceAccessFilter::LayoutsFilter)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this, [this](const QnResourcePtr& resource)
        {
            if (!resource.dynamicCast<QnLayoutResource>())
                return;

            /* Looks like hack as we have no dynamic filter model and must maintain list manually.
             * Really the only scenario we should handle is when the layout becomes remote (after it is saved). */
            connect(resource.data(), &QnResource::flagsChanged, this,
                &QnAccessibleResourcesWidget::handleResourceAdded);
        });
    }
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        disconnect(resource.data(), nullptr, this, nullptr);
        m_resourcesModel->removeResource(resource);
        if (resource == context()->user())
            refreshModel();
    });

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnAccessibleResourcesWidget::refreshModel);
    refreshModel();

    connect(m_resourcesModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    {
        if (roles.contains(Qt::CheckStateRole)
            && topLeft.column() <= QnResourceListModel::CheckColumn
            && bottomRight.column() >= QnResourceListModel::CheckColumn)
        {
            emit hasChangesChanged();
        }
    });
}

void QnAccessibleResourcesWidget::initSortFilterModel()
{
    auto updateFilter = [this]
    {
        QString textFilter = ui->filterLineEdit->text();

        /* Don't allow empty filters. */
        if (!textFilter.isEmpty() && textFilter.trimmed().isEmpty())
            ui->filterLineEdit->clear(); /*< Will call into this slot again. */
        else
            m_sortFilterModel->setFilterFixedString(textFilter);
    };

    m_sortFilterModel->setSourceModel(m_resourcesModel);
    m_sortFilterModel->sort(QnResourceListModel::NameColumn);
    m_sortFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortFilterModel->setFilterRole(Qn::ResourceSearchStringRole);
    m_sortFilterModel->setFilterKeyColumn(QnResourceListModel::NameColumn);
    m_sortFilterModel->setDynamicSortFilter(true);

    //TODO: #GDM #replace with SearchLineEdit
    ui->filterLineEdit->addAction(qnSkin->icon("theme/input_search.png"), QLineEdit::LeadingPosition);
    ui->filterLineEdit->setClearButtonEnabled(true);
    connect(ui->filterLineEdit, &QLineEdit::textChanged, this, updateFilter);
    connect(ui->filterLineEdit, &QLineEdit::editingFinished, this, updateFilter);
}

void QnAccessibleResourcesWidget::updateThumbnail(const QModelIndex& index)
{
    QModelIndex baseIndex = index.sibling(index.row(), QnAccessibleResourcesModel::NameColumn);
    QString toolTip = baseIndex.data(Qt::ToolTipRole).toString();
    ui->detailsWidget->setName(toolTip);
    ui->detailsWidget->setResource(index.data(Qn::ResourceRole).value<QnResourcePtr>());
    ui->detailsWidget->layout()->activate();
}

void QnAccessibleResourcesWidget::handleResourceAdded(const QnResourcePtr& resource)
{
    if (!resourcePassFilter(resource))
        return;

    if (m_resourcesModel->resources().contains(resource))
        return;

    m_resourcesModel->addResource(resource);
}

void QnAccessibleResourcesWidget::refreshModel()
{
    m_resourcesModel->setResources(QnResourceList());
    for (const QnResourcePtr& resource : resourcePool()->getResources())
        handleResourceAdded(resource);
    m_accessibleResourcesModel->setSubject(m_permissionsModel->subject());
}

void QnAccessibleResourcesWidget::at_itemViewKeyPress(QObject* watched, QEvent* event)
{
    NX_ASSERT(event->type() == QEvent::KeyPress);
    auto keyEvent = static_cast<QKeyEvent*>(event);

    if (watched == ui->resourcesTreeView)
    {
        if (ui->resourcesTreeView->currentIndex().row() != 0)
            return;

        switch (keyEvent->key())
        {
            case Qt::Key_Up:
            case Qt::Key_PageUp:
                ui->controlsTreeView->setFocus();
                event->accept();
                break;

            default:
                break;
        }
    }
    else if (watched == ui->controlsTreeView)
    {
        switch (keyEvent->key())
        {
            case Qt::Key_Down:
            case Qt::Key_PageDown:
                ui->resourcesTreeView->setCurrentIndex(ui->resourcesTreeView->model()->index(0, 0));
                ui->resourcesTreeView->setFocus();
                event->accept();
                break;

            default:
                break;
        }
    }
}

/* Shift-mouseclick selection process should set checkboxes
 * in all affected rows to the state the originating row has. */
void QnAccessibleResourcesWidget::handleSelectionChanging(
    QItemSelectionModel::SelectionFlags selectionFlags,
    const QModelIndex& index, const QEvent* event)
{
    const bool specialHandling = event && event->type() == QEvent::MouseButtonPress
        && static_cast<const QMouseEvent*>(event)->modifiers().testFlag(Qt::ShiftModifier);

    if (!specialHandling)
        return;

    const auto current = ui->resourcesTreeView->currentIndex(); //< originating item
    if (!current.isValid() || !index.isValid() || !selectionFlags.testFlag(QItemSelectionModel::Select))
        return;

    const auto newCheckValue = current.sibling(current.row(),
        QnAccessibleResourcesModel::CheckColumn).data(Qt::CheckStateRole);

    QAbstractItemModel* model = ui->resourcesTreeView->model();

    QPair<int, int> range(index.row(), current.row());
    if (range.first > range.second)
        qSwap(range.first, range.second);

    for (int i = range.first; i <= range.second; ++i)
    {
        model->setData(index.sibling(i, QnAccessibleResourcesModel::CheckColumn),
            newCheckValue, Qt::CheckStateRole);
    }
}

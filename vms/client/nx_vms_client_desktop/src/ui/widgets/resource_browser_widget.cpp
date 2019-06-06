#include "resource_browser_widget.h"
#include "ui_resource_browser_widget.h"

#include <QtCore/QItemSelectionModel>

#include <QtGui/QWheelEvent>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QGraphicsLinearLayout>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>

#include <client/client_runtime_settings.h>

#include <common/common_meta_types.h>
#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_property.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/utils/app_info.h>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/graphics_tooltip_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <ui/widgets/resource_tree_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/common/indents.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using NodeType = ResourceTreeNodeType;

namespace {

static const auto kStandardResourceTreeIndents = qVariantFromValue(QnIndents(2, 0));
static const auto kTaggedResourceTreeIndents = qVariantFromValue(QnIndents(8, 0));

const char* kSearchModelPropertyName = "_qn_searchModel";
const char* kSearchSynchronizerPropertyName = "_qn_searchSynchronizer";

const auto kHtmlLabelNoInfoFormat = lit("<center><span style='font-weight: 500'>%1</span></center>");
const auto kHtmlLabelDefaultFormat = lit("<center><span style='font-weight: 500'>%1</span> %2</center>");
const auto kHtmlLabelUserFormat = lit("<center><span style='font-weight: 500'>%1</span>, %2</center>");

static const QSize kMaxThumbnailSize(224, 184);

static void updateTreeItem(QnResourceTreeWidget* tree, const QnWorkbenchItem* item)
{
    if (!item)
        return;

    const auto resource = item->resource();
    NX_ASSERT(resource);
    if (!resource)
        return;

    tree->update(resource);
}

bool hasParentNodes(const QModelIndex& index)
{
    if (!index.isValid())
        return true; //< We suppose that real root node always contains parent nodes.

    const auto model = index.model();
    const int rowCount = model->rowCount(index);
    for (int row = 0; row != rowCount; ++row)
    {
        const auto childIndex = index.child(row, 0);
        if (model->rowCount(childIndex) > 0)
            return true;
    }
    return false;
}

bool isOpenableInEntity(const QnResourcePtr& resource)
{
    return QnResourceAccessFilter::isOpenableInEntity(resource)
        || QnResourceAccessFilter::isDroppable(resource);
}

ResourceTreeNodeType getNodeType(const QModelIndex& index)
{
    return index.data(Qn::NodeTypeRole).value<ResourceTreeNodeType>();;
}

bool isLocalResourcesGroup(const QModelIndex& index)
{
    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTreeNodeType>();
    return nodeType == ResourceTreeNodeType::localResources;
}

} // namespace

// -------------------------------------------------------------------------- //
// QnResourceBrowserWidget
// -------------------------------------------------------------------------- //
QnResourceBrowserWidget::QnResourceBrowserWidget(QWidget* parent, QnWorkbenchContext* context):
    QWidget(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::ResourceBrowserWidget()),
    m_tooltipWidget(nullptr),
    m_hoverProcessor(nullptr),
    m_thumbnailManager(new CameraThumbnailManager())
{
    ui->setupUi(this);

    const auto scrollBar = new SnappedScrollBar(ui->resourcesHolder);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    scrollBar->setUseMaximumSpace(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    // To keep aspect ratio specify only maximum height for server request
    m_thumbnailManager->setThumbnailSize(QSize(0, kMaxThumbnailSize.height()));

    m_resourceModel = new QnResourceTreeModel(QnResourceTreeModel::FullScope, this);
    ui->resourceTreeWidget->setModel(m_resourceModel);
    ui->resourceTreeWidget->setCheckboxesVisible(false);
    ui->resourceTreeWidget->setGraphicsTweaks(Qn::HideLastRow | Qn::BypassGraphicsProxy);
    ui->resourceTreeWidget->setEditingEnabled();
    ui->resourceTreeWidget->setAutoExpandPolicy(
        [](const QModelIndex& index)
        {
            switch (index.data(Qn::NodeTypeRole).value<NodeType>())
            {
                case NodeType::resource:
                {
                    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
                    return resource && resource->hasFlags(Qn::server);
                }
                case NodeType::servers:
                case NodeType::userResources:

                case NodeType::filteredServers:
                case NodeType::filteredCameras:
                case NodeType::filteredLayouts:
                case NodeType::filteredUsers:
                case NodeType::filteredVideowalls:
                    return true;
                default:
                    break;
            }
            return false;
        }
    );

    m_connections << connect(ui->resourceTreeWidget->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex& current, const QModelIndex& previous)
        {
            Q_UNUSED(previous);
            const auto treeView = ui->resourceTreeWidget->treeView();
            const auto itemRect = treeView->visualRect(current)
                .translated(treeView->mapTo(ui->scrollArea->widget(), QPoint()));
            ui->scrollArea->ensureVisible(0, itemRect.center().y() + 1, 0, itemRect.height() / 2);
        });

    ui->shortcutHintWidget->setContentsMargins(6, 0, 0, 0);
    ui->shortcutHintWidget->setVisible(false);
    ui->resourceTreeWidget->treeView()->setProperty(
        style::Properties::kSideIndentation, kStandardResourceTreeIndents);

    initInstantSearch();

    m_renameActions.insert(action::RenameResourceAction, new QAction(this));
    m_renameActions.insert(action::RenameVideowallEntityAction, new QAction(this));
    m_renameActions.insert(action::RenameLayoutTourAction, new QAction(this));

    setHelpTopic(this, Qn::MainWindow_Tree_Help);

    m_connections << connect(ui->resourceTreeWidget, &QnResourceTreeWidget::activated,
        this, &QnResourceBrowserWidget::handleItemActivated);

    m_connections << connect(ui->resourceTreeWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnResourceBrowserWidget::selectionChanged);

    /* Connect to context. */
    ui->resourceTreeWidget->setWorkbench(workbench());

    m_connections << connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged,
        this, &QnResourceBrowserWidget::at_workbench_currentLayoutAboutToBeChanged);
    m_connections << connect(workbench(), &QnWorkbench::currentLayoutChanged,
        this, &QnResourceBrowserWidget::at_workbench_currentLayoutChanged);
    m_connections << connect(workbench(), &QnWorkbench::itemAboutToBeChanged,
        this, &QnResourceBrowserWidget::at_workbench_itemChange);
    m_connections << connect(workbench(), &QnWorkbench::itemChanged,
        this, &QnResourceBrowserWidget::at_workbench_itemChange);

    m_connections << connect(accessController(),
        &QnWorkbenchAccessController::globalPermissionsChanged,
        this,
        &QnResourceBrowserWidget::updateIcons);

    m_connections << connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource == m_tooltipResource)
                setTooltipResource(QnResourcePtr());
        });

    installEventHandler({ui->scrollArea->verticalScrollBar()}, {QEvent::Show, QEvent::Hide},
        this, &QnResourceBrowserWidget::scrollBarVisibleChanged);

    m_connections << connect(this, &QnResourceBrowserWidget::scrollBarVisibleChanged, this,
        [this]()
        {
            auto margins = ui->scrollAreaWidgetContents->contentsMargins();
            margins.setRight(isScrollBarVisible() ?
                style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 0);
            ui->scrollAreaWidgetContents->setContentsMargins(margins);
        });

    /* Run handlers. */
    updateIcons();

    at_workbench_currentLayoutChanged();
}

QnResourceBrowserWidget::~QnResourceBrowserWidget()
{
    disconnect(workbench(), nullptr, this, nullptr);

    at_workbench_currentLayoutAboutToBeChanged();

    setTooltipResource(QnResourcePtr());
    ui->resourceTreeWidget->setWorkbench(nullptr);

    /* This class is one of the most significant reasons of crashes on exit. Workarounding it.. */
    m_connections = {};
}

using IndexCallback = std::function<void (const QModelIndex& index, bool hasChildren)>;
void forEachIndex(
    const QAbstractItemModel* model,
    const QModelIndex& root,
    const IndexCallback& callback)
{
    if (!callback)
    {
        NX_ASSERT(false, "Callback can't be null");
        return;
    }

    if (!model)
    {
        NX_ASSERT(false, "Model can't be null.");
        return;
    }

    const int childrenCount = model->rowCount(root);
    if (root.isValid())
        callback(root, childrenCount);

    if (childrenCount)
    {
        for (int row = 0; row != childrenCount; ++row)
            forEachIndex(model, model->index(row, 0, root), callback);
    }
}

void QnResourceBrowserWidget::initInstantSearch()
{
    const auto filterEdit = ui->instantFilterLineEdit;

    setPaletteColor(ui->nothingFoundLabel, QPalette::WindowText, colorTheme()->color("dark14"));
    setPaletteColor(ui->nothingFoundDescriptionLabel, QPalette::WindowText, colorTheme()->color("dark14"));

    connect(ui->resourceTreeWidget, &QnResourceTreeWidget::filterEnterPressed,
        this, [this] { handleEnterPressed(false);});
    connect(ui->resourceTreeWidget, &QnResourceTreeWidget::filterCtrlEnterPressed,
        this, [this] { handleEnterPressed(true);});

    // Initializes new filter edit

    connect(filterEdit, &SearchEdit::textChanged,
        this, &QnResourceBrowserWidget::updateInstantFilter);
    connect(filterEdit, &SearchEdit::editingFinished,
        this, &QnResourceBrowserWidget::updateInstantFilter);
    connect(filterEdit, &SearchEdit::currentTagDataChanged,
        this, &QnResourceBrowserWidget::updateInstantFilter);
    connect(filterEdit, &SearchEdit::focusedChanged,
        this, &QnResourceBrowserWidget::updateHintVisibilityByBasicState);

    connect(filterEdit, &SearchEdit::enterPressed,
        ui->resourceTreeWidget, &QnResourceTreeWidget::filterEnterPressed);
    connect(filterEdit, &SearchEdit::ctrlEnterPressed,
        ui->resourceTreeWidget, &QnResourceTreeWidget::filterCtrlEnterPressed);

    connect(commonModule(), &QnCommonModule::remoteIdChanged,
        this, &QnResourceBrowserWidget::updateSearchMode);

    const auto searchModel = ui->resourceTreeWidget->searchModel();

    connect(searchModel, &QAbstractItemModel::rowsInserted,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);
    connect(searchModel, &QAbstractItemModel::rowsRemoved,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);
    connect(searchModel, &QAbstractItemModel::modelReset,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);

    auto filterMenuCreator = [this](QWidget* parent) { return createFilterMenu(parent); };
    auto filterNameProvider = [this](const QVariant& data)
        { return getFilterName(data.value<ResourceTreeNodeType>()); };
    filterEdit->setTagOptionsSource(filterMenuCreator, filterNameProvider);

    updateSearchMode();
    handleInstantFilterUpdated();
}

void QnResourceBrowserWidget::handleEnterPressed(bool withControlKey)
{
    if (!ui->shortcutHintWidget->isVisible())
        return;

    if (!withControlKey && !m_hasOpenInLayoutItems)
        return;

    QnResourceList openableItems;
    QnVideoWallResourceList videowalls;
    QSet<QnUuid> showreels;

    // Used to prevent duplicates for openable resources and videowalls.
    QSet<QnResourcePtr> resources;

    const auto callback =
        [&](const QModelIndex& index, bool hasChildren)
        {
            if (getNodeType(index) == ResourceTreeNodeType::layoutTour)
            {
                showreels.insert(index.data(Qn::UuidRole).value<QnUuid>());
                return;
            }

            if (hasChildren)
                return;

            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            if (!resource)
                return;

            if (!QnResourceAccessFilter::isOpenableInLayout(resource)
                && !isOpenableInEntity(resource))
            {
                return;
            }

            if (resources.contains(resource))
                return;

            if (const auto videowall = resource.dynamicCast<QnVideoWallResource>())
                videowalls.append(videowall);
            else
                openableItems.append(resource); //< Keep sort order.

            resources.insert(resource); //< Avoid duplicates. //< TODO: keep uuid and for showreel
        };

    forEachIndex(ui->resourceTreeWidget->searchModel(), QModelIndex(), callback);

    if (!openableItems.isEmpty())
    {
        const auto action = withControlKey
            ? action::OpenInNewTabAction
            : action::OpenInCurrentLayoutAction;

        menu()->trigger(action, {openableItems});
    }

    if (withControlKey)
    {
        if (!videowalls.isEmpty())
        {
            for (const auto videowallResource: videowalls)
                menu()->triggerIfPossible(action::OpenVideoWallReviewAction, videowallResource);
        }

        if (!showreels.isEmpty())
        {
            for (const auto showreelId: showreels)
                menu()->triggerIfPossible(action::ReviewLayoutTourAction, {Qn::UuidRole, showreelId});
        }
    }
}

void QnResourceBrowserWidget::updateSearchMode()
{
    const auto filterEdit = ui->instantFilterLineEdit;
    const bool localResourcesMode = commonModule()->remoteGUID().isNull();

    filterEdit->setMenuEnabled(!localResourcesMode);
    filterEdit->setText(QString());
    filterEdit->setPlaceholderText(localResourcesMode
        ? tr("Local files")
        : tr("Search"));

    updateInstantFilter();
}

bool QnResourceBrowserWidget::updateFilteringMode(bool value)
{
    if (m_filtering == value)
        return false;

    m_filtering = value;
    return true;
}

void QnResourceBrowserWidget::storeExpandedStates()
{
    if (!m_expandedStatesList.isEmpty())
    {
        NX_ASSERT(false, "Expanded states list should be empty.");
        m_expandedStatesList.clear();
    }

    const auto tree = ui->resourceTreeWidget->treeView();
    const auto searchModel = ui->resourceTreeWidget->searchModel();
    const auto callback =
        [this, tree, searchModel](const QModelIndex& index, bool hasChildren)
        {
            if (!hasChildren)
                return;

            const auto searchIndex = searchModel->mapFromSource(index);
            m_expandedStatesList.append(ExpandedState(index, tree->isExpanded(searchIndex)));
        };

    forEachIndex(ui->resourceTreeWidget->model(), QModelIndex(), callback);
}

void QnResourceBrowserWidget::restoreExpandedStates()
{
    const auto tree = ui->resourceTreeWidget->treeView();
    const auto searchModel = ui->resourceTreeWidget->searchModel();
    for (const auto& data: m_expandedStatesList)
    {
        const auto& index = data.first;
        const auto searchIndex = searchModel->mapFromSource(index);
        if (searchIndex.isValid())
            tree->setExpanded(searchIndex, data.second);
    }
    m_expandedStatesList.clear();
}

void QnResourceBrowserWidget::updateInstantFilter()
{
    const auto filterEdit = ui->instantFilterLineEdit;
    const auto queryText = filterEdit->text();

    /* Don't allow empty filters. */
    const auto trimmed = queryText.trimmed();

    if (trimmed.isEmpty())
        filterEdit->clear();

    const bool localResourcesMode = commonModule()->remoteGUID().isNull();
    const auto allowedNode =
        [this, localResourcesMode, filterEdit]()
        {
            if (!filterEdit->currentTagData().isNull())
                return filterEdit->currentTagData().value<ResourceTreeNodeType>();

            return localResourcesMode
                ? QnResourceSearchQuery::NodeType::localResources
                : QnResourceSearchQuery::kAllowAllNodeTypes;
        }();

    const bool filtering = !trimmed.isEmpty()
        || (!localResourcesMode && !filterEdit->currentTagData().isNull());
    const bool filteringUpdated = updateFilteringMode(filtering);
    if (filteringUpdated && filtering)
        storeExpandedStates();

    const auto searchModel = ui->resourceTreeWidget->searchModel();
    const auto newRootNode = searchModel->setQuery(QnResourceSearchQuery(trimmed, allowedNode));
    const auto treeView = ui->resourceTreeWidget->treeView();
    treeView->setRootIndex(newRootNode);
    if (filtering)
        treeView->expandAll();

    if (filteringUpdated && !filtering)
        restoreExpandedStates();

    handleInstantFilterUpdated();
}

void QnResourceBrowserWidget::handleInstantFilterUpdated()
{
    const auto searchModel = ui->resourceTreeWidget->searchModel();
    const auto treeView = ui->resourceTreeWidget->treeView();
    const auto rootIndex = treeView->rootIndex();
    const bool emptyResults = searchModel->rowCount(rootIndex) == 0;
    const bool noLocalFiles = commonModule()->remoteGUID().isNull()
        && searchModel->query().text.trimmed().isEmpty()
        && emptyResults;
    ui->nothingFoundDescriptionLabel->setVisible(noLocalFiles);
    ui->nothingFoundLabel->setText(noLocalFiles ? tr("No local files") : tr("Nothing found"));

    static constexpr int kResourcesPage = 0;
    static constexpr int kNothingFoundPage = 1;
    ui->resourcesHolder->setCurrentIndex(emptyResults ? kNothingFoundPage : kResourcesPage);

    treeView->setRootIsDecorated(hasParentNodes(treeView->rootIndex()));
    const auto indents = treeView->rootIsDecorated()
        ? kStandardResourceTreeIndents
        : kTaggedResourceTreeIndents;
    treeView->setProperty(style::Properties::kSideIndentation, indents);

    updateHintVisibilityByBasicState();
    updateHintVisibilityByFilterState();
}

void QnResourceBrowserWidget::setHintVisible(bool value)
{
    if (ui->shortcutHintWidget->isVisible() != value)
         ui->shortcutHintWidget->setVisible(value);
}

void QnResourceBrowserWidget::setAvailableItemTypes(
    bool hasOpenInLayoutItems,
    bool hasOpenInEntityItems,
    bool hasUnopenableItems)
{
    static const ShortcutHintWidget::DescriptionList kOpenInLayoutHints({
        { QKeySequence(Qt::Key_Enter), tr("add to current layout") },
        { QKeySequence(Qt::Key_Control, Qt::Key_Enter), tr("open all at a new layout") } });
    static const ShortcutHintWidget::DescriptionList kOpenEntitiesHint({
        { QKeySequence(Qt::Key_Control, Qt::Key_Enter), tr("open all") } });

    const bool hasChanges =
        m_hasOpenInLayoutItems != hasOpenInLayoutItems
        || m_hasOpenInEntityItems != hasOpenInEntityItems
        || m_hasUnopenableItems != hasUnopenableItems;

    if (!hasChanges)
        return;

    m_hasOpenInLayoutItems = hasOpenInLayoutItems;
    m_hasOpenInEntityItems = hasOpenInEntityItems;
    m_hasUnopenableItems = hasUnopenableItems;

    const auto hintWidget = ui->shortcutHintWidget;
    if (hasOpenInEntityItems != hasOpenInLayoutItems)
        hintWidget->setDescriptions(hasOpenInLayoutItems ? kOpenInLayoutHints : kOpenEntitiesHint);

    setHintVisible(hintIsVisbleByFilterState() && hintIsVisibleByBasicState());
}

bool QnResourceBrowserWidget::hintIsVisbleByFilterState() const
{
    return !m_hasUnopenableItems && m_hasOpenInEntityItems != m_hasOpenInLayoutItems;
}

void QnResourceBrowserWidget::updateHintVisibilityByFilterState()
{
    bool hasOpenInLayoutItems = false;
    bool hasOpenInEntityItems = false;
    bool hasUnopenableItems = false;

    const auto hintIsDefenetelyInvisible =
        [&hasOpenInLayoutItems, &hasOpenInEntityItems, &hasUnopenableItems]()
        {
            return hasUnopenableItems
                || (hasOpenInLayoutItems && hasOpenInLayoutItems == hasOpenInEntityItems);
        };

    const auto searchModel = ui->resourceTreeWidget->searchModel();
    const auto iterateGroup =
        [searchModel, hintIsDefenetelyInvisible, &hasOpenInLayoutItems,
            &hasOpenInEntityItems, &hasUnopenableItems]
            (const QModelIndex& groupIndex)
        {
            const bool hasSingleOpenTypeItems =
                getNodeType(groupIndex) != ResourceTreeNodeType::localResources;

            const int childrenCount = searchModel->rowCount(groupIndex);
            for (int childRow = 0; childRow != childrenCount; ++childRow)
            {
                const auto index = groupIndex.child(childRow, 0);
                if (const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>())
                {
                    if (QnResourceAccessFilter::isOpenableInLayout(resource))
                        hasOpenInLayoutItems = true;
                    else if (isOpenableInEntity(resource))
                        hasOpenInEntityItems = true;
                    else
                        hasUnopenableItems = true;
                }
                else
                {
                    switch(getNodeType(index))
                    {
                        case ResourceTreeNodeType::layoutTour:
                            hasOpenInEntityItems = true;
                            break;
                        case ResourceTreeNodeType::recorder:
                            hasOpenInLayoutItems |= searchModel->rowCount(index) > 0;
                            break;
                        default:
                            break;
                    }
                }

                if (hasSingleOpenTypeItems || hintIsDefenetelyInvisible())
                    break;
            }
        };

    const auto rootIndex = ui->resourceTreeWidget->treeView()->rootIndex();
    if (rootIndex.isValid())
    {
        iterateGroup(rootIndex);
    }
    else
    {
        const auto groupsCount = searchModel->rowCount();
        for (int groupRow = 0; groupRow != groupsCount; ++groupRow)
        {
            const auto groupIndex = searchModel->index(groupRow, 0);
            iterateGroup(groupIndex);

            if (hintIsDefenetelyInvisible())
                break;
        }
    }

    setAvailableItemTypes(hasOpenInLayoutItems, hasOpenInEntityItems, hasUnopenableItems);
}

bool QnResourceBrowserWidget::hintIsVisibleByBasicState() const
{
    return m_hintIsVisibleByBasicState;
}

void QnResourceBrowserWidget::setHintVisibleByBasicState(bool value)
{
    if (m_hintIsVisibleByBasicState == value)
        return;

    m_hintIsVisibleByBasicState = value;
    setHintVisible(hintIsVisbleByFilterState() && hintIsVisibleByBasicState());
}

void QnResourceBrowserWidget::updateHintVisibilityByBasicState()
{
    const auto searchModel = ui->resourceTreeWidget->searchModel();
    const auto query = searchModel->query();
    const bool hasFilterText = !query.text.trimmed().isEmpty();
    const bool hasSelectedGroup = query.allowedNode != QnResourceSearchQuery::kAllowAllNodeTypes;

    setHintVisibleByBasicState(
        (hasFilterText || hasSelectedGroup)
        && ui->instantFilterLineEdit->focused());
}

QMenu* QnResourceBrowserWidget::createFilterMenu(QWidget* parent) const
{
    QMenu* result = new QMenu(parent);

    auto escapeActionText =
        [](const QString& text)
        {
            return QString(text).replace(lit("&"), lit("&&"));
        };

    auto addMenuItem =
        [this, result, escapeActionText](ResourceTreeNodeType filterNodeType)
        {
            auto action = result->addAction(escapeActionText(getFilterName(filterNodeType)));
            action->setData(QVariant::fromValue(filterNodeType));
        };

    const QList<ResourceTreeNodeType> filterNodeOptions = {
        ResourceTreeNodeType::filteredServers,
        ResourceTreeNodeType::filteredCameras,
        ResourceTreeNodeType::filteredLayouts,
        ResourceTreeNodeType::layoutTours,
        ResourceTreeNodeType::filteredVideowalls,
        ResourceTreeNodeType::webPages,
        ResourceTreeNodeType::filteredUsers,
        ResourceTreeNodeType::localResources};

    addMenuItem(QnResourceSearchQuery::kAllowAllNodeTypes);
    result->addSeparator();
    for (auto filterNodeType: filterNodeOptions)
    {
        if (!m_resourceModel->rootNode(filterNodeType)->children().isEmpty())
            addMenuItem(filterNodeType);
    }

    return result;
}

QString QnResourceBrowserWidget::getFilterName(ResourceTreeNodeType allowedNodeType) const
{
    switch (allowedNodeType)
    {
        case QnResourceSearchQuery::kAllowAllNodeTypes:
            return tr("All types");
        case ResourceTreeNodeType::filteredServers:
            return tr("Servers");
        case ResourceTreeNodeType::filteredCameras:
            return tr("Cameras & Devices");
        case ResourceTreeNodeType::filteredLayouts:
            return tr("Layouts");
        case ResourceTreeNodeType::layoutTours:
            return tr("Showreels");
        case ResourceTreeNodeType::filteredVideowalls:
            return tr("Video Walls");
        case ResourceTreeNodeType::webPages:
            return tr("Web Pages");
        case ResourceTreeNodeType::filteredUsers:
            return tr("Users");
        case ResourceTreeNodeType::localResources:
            return tr("Local Files");
        default:
            NX_ASSERT(false);
            break;
    }
    return QString();
}

bool QnResourceBrowserWidget::isLayoutSearchable(QnWorkbenchLayout* layout) const
{
    // Search results must not be displayed over layout tours, preview layouts and videowalls.
    const auto permissions = Qn::WritePermission | Qn::AddRemoveItemsPermission;
    return accessController()->hasPermissions(layout->resource(), permissions)
        && layout->data(Qn::LayoutTourUuidRole).value<QnUuid>().isNull();
}

QnResourceSearchProxyModel* QnResourceBrowserWidget::layoutModel(QnWorkbenchLayout* layout, bool create) const
{
    auto result = layout->property(kSearchModelPropertyName).value<QnResourceSearchProxyModel*>();
    if (create && !result)
    {
        result = new QnResourceSearchProxyModel(layout);
        result->setFilterCaseSensitivity(Qt::CaseInsensitive);
        result->setFilterKeyColumn(0);
        result->setSortCaseSensitivity(Qt::CaseInsensitive);
        result->setDynamicSortFilter(true);
        result->setSourceModel(m_resourceModel);
        result->setDefaultBehavior(QnResourceSearchProxyModel::DefaultBehavior::showNone);

        layout->setProperty(kSearchModelPropertyName, QVariant::fromValue<QnResourceSearchProxyModel*>(result));
    }
    return result;
}

QnResourceSearchSynchronizer* QnResourceBrowserWidget::layoutSynchronizer(QnWorkbenchLayout* layout, bool create) const
{
    auto result = layout->property(kSearchSynchronizerPropertyName).value<QnResourceSearchSynchronizer*>();
    if (create && !result && isLayoutSearchable(layout))
    {
        result = new QnResourceSearchSynchronizer(layout);
        result->setLayout(layout);
        result->setModel(layoutModel(layout, true));

        layout->setProperty(kSearchSynchronizerPropertyName, QVariant::fromValue<QnResourceSearchSynchronizer*>(result));
    }
    return result;
}

void QnResourceBrowserWidget::showContextMenuAt(const QPoint& pos, bool ignoreSelection)
{
    if (!context() || !context()->menu())
    {
        qnWarning("Requesting context menu for a tree widget while no menu manager instance is available.");
        return;
    }

    if (qnRuntime->isVideoWallMode())
        return;

    auto manager = context()->menu();

    QScopedPointer<QMenu> menu(manager->newMenu(action::TreeScope, nullptr, ignoreSelection
        ? action::Parameters{Qn::NodeTypeRole, NodeType::root}
        : currentParameters(action::TreeScope)));

    for (action::IDType key : m_renameActions.keys())
        manager->redirectAction(menu.data(), key, m_renameActions[key]);

    if (menu->isEmpty())
        return;

    /* Run menu. */
    QAction* action = QnHiDpiWorkarounds::showMenu(menu.data(), pos);

    /* Process tree-local actions. */
    if (m_renameActions.values().contains(action))
        currentTreeWidget()->edit();
}

QnResourceTreeWidget* QnResourceBrowserWidget::currentTreeWidget() const
{
    return ui->resourceTreeWidget;
}

QItemSelectionModel* QnResourceBrowserWidget::currentSelectionModel() const
{
    return currentTreeWidget()->selectionModel();
}

QModelIndex QnResourceBrowserWidget::itemIndexAt(const QPoint& pos) const
{
    QAbstractItemView* treeView = currentTreeWidget()->treeView();
    if (!treeView->model())
        return QModelIndex();
    QPoint childPos = treeView->mapFrom(const_cast<QnResourceBrowserWidget*>(this), pos);
    return treeView->indexAt(childPos);
}

QnResourceList QnResourceBrowserWidget::selectedResources() const
{
    QnResourceList result;

    for (const QModelIndex& index : currentSelectionModel()->selectedRows())
    {
        const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

        switch (nodeType)
        {
            case NodeType::recorder:
            {
                for (int i = 0; i < index.model()->rowCount(index); i++)
                {
                    QModelIndex subIndex = index.model()->index(i, 0, index);
                    QnResourcePtr resource = subIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
                    if (resource && !result.contains(resource))
                        result.append(resource);
                }
            }
            break;
            case NodeType::resource:
            case NodeType::sharedLayout:
            case NodeType::sharedResource:
            case NodeType::edge:
            case NodeType::currentUser:
            {
                QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
                if (resource && !result.contains(resource))
                    result.append(resource);
            }
            break;
            default:
                break;
        }
    }

    return result;
}

QnLayoutItemIndexList QnResourceBrowserWidget::selectedLayoutItems() const
{
    QnLayoutItemIndexList result;

    foreach(const QModelIndex& index, currentSelectionModel()->selectedRows())
    {
        QnUuid uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();
        if (uuid.isNull())
            continue;

        QnLayoutResourcePtr layout = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnLayoutResource>();
        if (!layout)
            continue;

        result.push_back(QnLayoutItemIndex(layout, uuid));
    }

    return result;
}

QnVideoWallItemIndexList QnResourceBrowserWidget::selectedVideoWallItems() const
{
    QnVideoWallItemIndexList result;

    foreach(const QModelIndex& modelIndex, currentSelectionModel()->selectedRows())
    {
        QnUuid uuid = modelIndex.data(Qn::ItemUuidRole).value<QnUuid>();
        if (uuid.isNull())
            continue;
        QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(uuid);
        if (!index.isNull())
            result.push_back(index);
    }

    return result;
}

QnVideoWallMatrixIndexList QnResourceBrowserWidget::selectedVideoWallMatrices() const
{
    QnVideoWallMatrixIndexList result;

    foreach(const QModelIndex& modelIndex, currentSelectionModel()->selectedRows())
    {
        QnUuid uuid = modelIndex.data(Qn::ItemUuidRole).value<QnUuid>();
        if (uuid.isNull())
            continue;

        QnVideoWallMatrixIndex index = resourcePool()->getVideoWallMatrixByUuid(uuid);
        if (!index.isNull())
            result.push_back(index);
    }

    return result;
}

action::ActionScope QnResourceBrowserWidget::currentScope() const
{
    return action::TreeScope;
}

QString QnResourceBrowserWidget::toolTipAt(const QPointF& pos) const
{
    Q_UNUSED(pos);
    return QString(); // default tooltip should not be displayed anyway
}

bool QnResourceBrowserWidget::showOwnTooltip(const QPointF& pos)
{
    if (!m_tooltipWidget)
        return true; // default tooltip should not be displayed anyway

    const QModelIndex index = itemIndexAt(pos.toPoint());
    if (!index.isValid())
        return true;

    const QString toolTipText = index.data(Qt::ToolTipRole).toString();
    if (toolTipText.isEmpty())
    {
        hideToolTip();
    }
    else
    {
        const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        const auto extraInfo = QnResourceDisplayInfo(resource).extraInfo();

        const auto userDisplayedName =
            [&resource](const QString& defaultName)
            {
                if (const auto user = resource.dynamicCast<QnUserResource>())
                {
                    const auto name = user->fullName().trimmed();
                    if (!name.isEmpty())
                        return name;
                }

                return defaultName;
            };

        QString text;
        if (extraInfo.isEmpty())
            text = kHtmlLabelNoInfoFormat.arg(toolTipText);
        else if (resource && resource->hasFlags(Qn::user))
            text = kHtmlLabelUserFormat.arg(userDisplayedName(toolTipText)).arg(extraInfo);
        else
            text = kHtmlLabelDefaultFormat.arg(toolTipText).arg(extraInfo);

        m_tooltipWidget->setText(text);
        m_tooltipWidget->pointTo(QPointF(geometry().right(), pos.y()));

        setTooltipResource(resource);
        showToolTip();
    }

    return true;
}

void QnResourceBrowserWidget::setToolTipParent(QGraphicsWidget* widget)
{
    if (m_tooltipWidget)
        return;

    m_tooltipWidget = new QnGraphicsToolTipWidget(widget);
    m_tooltipWidget->setMaxThumbnailSize(kMaxThumbnailSize);
    m_tooltipWidget->setImageProvider(m_thumbnailManager.data());

    m_hoverProcessor = new HoverFocusProcessor(widget);

    m_tooltipWidget->setFocusProxy(widget);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);

    //    m_tooltipWidget->installEventFilter(item);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget, &QnGraphicsToolTipWidget::thumbnailClicked, this,
        &QnResourceBrowserWidget::at_thumbnailClicked);
    connect(m_tooltipWidget, &QnToolTipWidget::tailPosChanged, this,
        &QnResourceBrowserWidget::updateToolTipPosition);
    connect(widget, &QGraphicsWidget::geometryChanged, this,
        &QnResourceBrowserWidget::updateToolTipPosition);

    m_hoverProcessor->addTargetItem(widget);
    m_hoverProcessor->addTargetItem(m_tooltipWidget);
    m_hoverProcessor->setHoverEnterDelay(250);
    m_hoverProcessor->setHoverLeaveDelay(250);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnResourceBrowserWidget::hideToolTip);

    updateToolTipPosition();
}

bool QnResourceBrowserWidget::isScrollBarVisible() const
{
    return ui->scrollArea->verticalScrollBar()->isVisible();
}

action::Parameters QnResourceBrowserWidget::currentParameters(action::ActionScope scope) const
{
    if (scope != action::TreeScope)
        return action::Parameters();

    // TODO: #GDM #3.1 refactor to a simple switch by node type

    QItemSelectionModel* selectionModel = currentSelectionModel();
    QModelIndex index = selectionModel->currentIndex();

    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

    auto withNodeType = [nodeType](action::Parameters parameters)
        {
            return parameters.withArgument(Qn::NodeTypeRole, nodeType);
        };

    switch (nodeType)
    {
        case NodeType::videoWallItem:
            return withNodeType(selectedVideoWallItems());
        case NodeType::videoWallMatrix:
            return withNodeType(selectedVideoWallMatrices());
        case NodeType::cloudSystem:
        {
            action::Parameters result{Qn::CloudSystemIdRole,
                index.data(Qn::CloudSystemIdRole).toString()};
            return withNodeType(result);
        }
        case NodeType::layoutItem:
            return withNodeType(selectedLayoutItems());

        default:
            break;
    }

    action::Parameters result(selectedResources());

    /* For working with shared layout links we must know owning user resource. */
    QModelIndex parentIndex = index.parent();
    const auto parentNodeType = parentIndex.data(Qn::NodeTypeRole).value<NodeType>();

    /* We can select several layouts and some other resources in any part of tree - in this case just do not set anything. */
    QnUserResourcePtr user;
    QnUuid uuid = index.data(Qn::UuidRole).value<QnUuid>();

    switch (nodeType)
    {
        case NodeType::sharedLayouts:
            user = parentIndex.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
            uuid = parentIndex.data(Qn::UuidRole).value<QnUuid>();
            break;
        default:
            break;
    }

    switch (parentNodeType)
    {
        case NodeType::layouts:
            user = context()->user();
            break;
        case NodeType::sharedResources:
        case NodeType::sharedLayouts:
            user = parentIndex.parent().data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
            uuid = parentIndex.parent().data(Qn::UuidRole).value<QnUuid>();
            break;
        case NodeType::resource:
            user = parentIndex.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
            break;
        default:
            break;
    }

    if (user)
        result.setArgument(Qn::UserResourceRole, user);

    if (!uuid.isNull())
        result.setArgument(Qn::UuidRole, uuid);

    result.setArgument(Qn::NodeTypeRole, nodeType);
    return result;
}

void QnResourceBrowserWidget::hideToolTip()
{
    if (!m_tooltipWidget)
        return;

    using namespace nx::vms::client::desktop::ui::workbench;
    //todo: #GDM add parameter 'animated'
    if (hasOpacityAnimator(m_tooltipWidget))
    {
        auto animator = opacityAnimator(m_tooltipWidget);
        qnWorkbenchAnimations->setupAnimator(animator, Animations::Id::ResourcesPanelTooltipHide);
        animator->animateTo(0.0);
    }
    else
    {
        m_tooltipWidget->setOpacity(0.0);
    }
}

void QnResourceBrowserWidget::showToolTip()
{
    if (!m_tooltipWidget)
        return;

    using namespace nx::vms::client::desktop::ui::workbench;

    auto animator = opacityAnimator(m_tooltipWidget);
    qnWorkbenchAnimations->setupAnimator(animator, Animations::Id::ResourcesPanelTooltipShow);
    animator->animateTo(1.0);
}

void QnResourceBrowserWidget::clearSelection()
{
    currentSelectionModel()->clear();
}

void QnResourceBrowserWidget::updateIcons()
{
    QnResourceItemDelegate::Options opts = QnResourceItemDelegate::RecordingIcons;
    if (accessController()->hasGlobalPermission(GlobalPermission::editCameras))
        opts |= QnResourceItemDelegate::ProblemIcons;

    ui->resourceTreeWidget->itemDelegate()->setOptions(opts);
}

void QnResourceBrowserWidget::updateToolTipPosition()
{
    if (!m_tooltipWidget)
        return;
    m_tooltipWidget->updateTailPos();
    //m_tooltipWidget->pointTo(QPointF(qRound(geometry().right()), qRound(geometry().height() / 2 )));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceBrowserWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QWidget* child = childAt(event->pos());
    while (child && child != ui->resourceTreeWidget)
        child = child->parentWidget();

    /**
     * Note that we cannot use event->globalPos() here as it doesn't work when
     * the widget is embedded into graphics scene.
     */
    showContextMenuAt(QCursor::pos(), !child);
}

void QnResourceBrowserWidget::wheelEvent(QWheelEvent* event)
{
    event->accept(); /* Do not propagate wheel events past the tree widget. */
}

void QnResourceBrowserWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceBrowserWidget::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnResourceBrowserWidget::keyPressEvent(QKeyEvent* event)
{
    event->accept();
    if (event->key() == Qt::Key_Menu)
    {
        QPoint pos = currentTreeWidget()->selectionPos();
        if (pos.isNull())
            return;

        showContextMenuAt(pos);
    }
}

void QnResourceBrowserWidget::keyReleaseEvent(QKeyEvent* event)
{
    event->accept();
}

void QnResourceBrowserWidget::at_workbench_currentLayoutAboutToBeChanged()
{
    auto layout = workbench()->currentLayout();
    layout->disconnect(this);

    if (auto synchronizer = layoutSynchronizer(layout, false))
        synchronizer->disableUpdates();
}

void QnResourceBrowserWidget::at_workbench_currentLayoutChanged()
{
    auto layout = workbench()->currentLayout();

    if (auto synchronizer = layoutSynchronizer(layout, false))
        synchronizer->enableUpdates();

    /* Bold state has changed. */
    currentTreeWidget()->update();

    connect(layout, &QnWorkbenchLayout::itemAdded, this,
        &QnResourceBrowserWidget::at_layout_itemAdded);
    connect(layout, &QnWorkbenchLayout::itemRemoved, this,
        &QnResourceBrowserWidget::at_layout_itemRemoved);
}

void QnResourceBrowserWidget::at_workbench_itemChange(Qn::ItemRole role)
{
    /* Raised state has changed. */
    updateTreeItem(currentTreeWidget(), workbench()->item(role));
}

void QnResourceBrowserWidget::at_layout_itemAdded(QnWorkbenchItem* item)
{
    /* Bold state has changed. */
    updateTreeItem(currentTreeWidget(), item);
}

void QnResourceBrowserWidget::at_layout_itemRemoved(QnWorkbenchItem* item)
{
    /* Bold state has changed. */
    updateTreeItem(currentTreeWidget(), item);
}

void QnResourceBrowserWidget::at_thumbnailClicked()
{
    if (!m_tooltipWidget || !m_tooltipResource)
        return;

    menu()->trigger(action::OpenInCurrentLayoutAction, m_tooltipResource);
}

void QnResourceBrowserWidget::handleItemActivated(const QModelIndex& index, bool withMouse)
{
    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

    if (nodeType == NodeType::cloudSystem)
    {
        const auto callback =
            [this, cloudSystemId = index.data(Qn::CloudSystemIdRole).toString()]()
            {
                menu()->trigger(action::ConnectToCloudSystemAction,
                    {Qn::CloudSystemIdRole, cloudSystemId});
            };

        // Looks like Qt has some bug when mouse events hangs and are targeted to
        // the wrong widget if we switch between QGLWidget-based views and QQuickView.
        // This workaround gives time to all (double)click-related events to be processed.
        if (nx::utils::AppInfo::isMacOsX())
        {
            static constexpr int kSomeEnoughDelayToProcessDoubleClick = 100;
            executeDelayedParented(callback, kSomeEnoughDelayToProcessDoubleClick, this);
        }
        else
        {
            callback();
        }
        return;
    }

    if (nodeType == NodeType::videoWallItem)
    {
        auto item = resourcePool()->getVideoWallItemByUuid(index.data(Qn::UuidRole).value<QnUuid>());
        menu()->triggerIfPossible(action::StartVideoWallControlAction,
            QnVideoWallItemIndexList() << item);
        return;
    }

    if (nodeType == NodeType::layoutTour)
    {
        menu()->triggerIfPossible(action::ReviewLayoutTourAction,
            {Qn::UuidRole, index.data(Qn::UuidRole).value<QnUuid>()});
        return;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    /* Do not open users or fake servers. */
    if (!resource || resource->hasFlags(Qn::user) || resource->hasFlags(Qn::fake))
        return;

    /* Do not open servers of admin.  */
    if (nodeType == NodeType::resource && resource->hasFlags(Qn::server) && withMouse)
        return;

    const bool isLayoutTourReviewMode = workbench()->currentLayout()->isLayoutTourReview();
    const auto actionId = isLayoutTourReviewMode
        ? action::OpenInNewTabAction
        : action::DropResourcesAction;
    menu()->trigger(actionId, resource);
}

void QnResourceBrowserWidget::setTooltipResource(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    m_tooltipResource = camera;
    m_thumbnailManager->selectCamera(camera);
    m_tooltipWidget->setThumbnailVisible(!camera.isNull());
}

void QnResourceBrowserWidget::selectNodeByUuid(const QnUuid& id)
{
    NX_ASSERT(!id.isNull());
    if (id.isNull())
        return;

    auto model = ui->resourceTreeWidget->searchModel();
    NX_ASSERT(model);
    if (!model)
        return;

    selectIndices(model->match(model->index(0, 0),
        Qn::UuidRole,
        qVariantFromValue(id),
        1,
        Qt::MatchExactly | Qt::MatchRecursive));
}

void QnResourceBrowserWidget::selectResource(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    auto model = ui->resourceTreeWidget->searchModel();
    NX_ASSERT(model);
    if (!model)
        return;

    selectIndices(model->match(model->index(0, 0),
        Qn::ResourceRole,
        qVariantFromValue(resource),
        1,
        Qt::MatchExactly | Qt::MatchRecursive));
}

void QnResourceBrowserWidget::selectIndices(const QModelIndexList& indices)
{
    if (indices.empty())
        return;

    for (const auto& index: indices)
    {
        auto parent = index.parent();
        while (parent.isValid())
        {
            ui->resourceTreeWidget->expand(parent);
            parent = parent.parent();
        }
    }

    if (auto selection = ui->resourceTreeWidget->selectionModel())
    {
        selection->clear();
        for (const auto& index: indices)
            selection->select(index, QItemSelectionModel::Select);
    }
}

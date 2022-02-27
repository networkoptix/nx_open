// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_browser_widget.h"
#include "ui_resource_browser_widget.h"

#include <QtCore/QItemSelectionModel>

#include <QtGui/QWheelEvent>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeView>

#include <api/global_settings.h>

#include <client/client_runtime_settings.h>

#include <common/common_meta_types.h>
#include <common/common_module.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_composer.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_drag_drop_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_interaction_handler.h>
#include <nx/vms/client/desktop/resource_views/item_view_drag_and_drop_scroll_assist.h>
#include <nx/vms/client/desktop/resource_views/grouped_resources_view_state_keeper.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_edit_delegate.h>

#include <ui/animation/widget_opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/processors/hover_processor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/workbench/widgets/deprecated_thumbnail_tooltip.h>
#include <ui/widgets/resource_tree_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/indents.h>
#include <ui/workbench/handlers/workbench_action_handler.h>

#include <utils/common/event_processors.h>
#include <nx/build_info.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using NodeType = ResourceTree::NodeType;

namespace {

static const auto kStandardResourceTreeIndents = QVariant::fromValue(QnIndents(2, 0));
static const auto kTaggedResourceTreeIndents = QVariant::fromValue(QnIndents(8, 0));

const char* kSearchModelPropertyName = "_qn_searchModel";
const char* kSearchSynchronizerPropertyName = "_qn_searchSynchronizer";

const auto kHtmlLabelNoInfoFormat =
    lit("<center><span style='font-weight: 500'>%1</span></center>");

const auto kHtmlLabelDefaultFormat =
    lit("<center><span style='font-weight: 500'>%1</span> %2</center>");

const auto kHtmlLabelUserFormat =
    lit("<center><span style='font-weight: 500'>%1</span>, %2</center>");

static const QSize kMaxThumbnailSize(224, 184);

static constexpr int kMaxAutoExpandedServers = 2;

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

ResourceTree::NodeType getNodeType(const QModelIndex& index)
{
    return index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnResourceBrowserWidget definition.
//-------------------------------------------------------------------------------------------------
QnResourceBrowserWidget::QnResourceBrowserWidget(QnWorkbenchContext* context, QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::ResourceBrowserWidget()),
    m_resourceTreeComposer(new entity_resource_tree::ResourceTreeComposer(
        context->commonModule(), context->accessController(), context->resourceTreeSettings())),
    m_resourceModel(new entity_item_model::EntityItemModel()),
    m_dragDropDecoratorModel(new ResourceTreeDragDropDecoratorModel(
        context->resourcePool(),
        context->menu(),
        context->instance<ui::workbench::ActionHandler>())),
    m_iconDecoratorModel(new ResourceTreeIconDecoratorModel()),
    m_tooltip(new DeprecatedThumbnailTooltip(Qt::LeftEdge, parentWidget())),
    m_opacityAnimator(widgetOpacityAnimator(m_tooltip, display()->instrumentManager()->animationTimer())),
    m_hoverProcessor(new HoverFocusProcessor()),
    m_thumbnailManager(new CameraThumbnailManager())
{
    ui->setupUi(this);
    setPaletteColor(this, QPalette::Mid, colorTheme()->color("dark8"));

    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ItemViewDragAndDropScrollAssist::setupScrollAssist(ui->scrollArea, treeView());

    // To keep aspect ratio specify only maximum height for server request
    m_thumbnailManager->setThumbnailSize(QSize(0, kMaxThumbnailSize.height()));

    m_resourceTreeComposer->attachModel(m_resourceModel.get());
    m_resourceModel->setEditDelegate(ResourceTreeEditDelegate(context));
    m_dragDropDecoratorModel->setSourceModel(m_resourceModel.get());
    m_iconDecoratorModel->setSourceModel(m_dragDropDecoratorModel.get());

    treeWidget()->setModel(m_iconDecoratorModel.get());
    setupAutoExpandPolicy();

    m_interactionHandler.reset(new ResourceTreeInteractionHandler(context));
    m_groupedResourcesViewState.reset(new GroupedResourcesViewStateKeeper(
        context, treeView(), ui->scrollArea, m_resourceModel.get()));

    m_connections << connect(
        m_interactionHandler.get(), &ResourceTreeInteractionHandler::editRequested,
        this, [this]() { treeWidget()->edit(); });

    m_connections << connect(treeWidget()->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex& current, const QModelIndex& /*previous*/)
        {
            if (!current.isValid())
                return;

            const auto itemRect = treeView()->visualRect(current)
                .translated(treeView()->mapTo(ui->scrollArea->widget(), QPoint()));

            if (itemRect.isNull())
                return;

            ui->scrollArea->ensureVisible(0, itemRect.center().y() + 1, 0, itemRect.height() / 2);
        });

    treeView()->setProperty(nx::style::Properties::kSideIndentation, kStandardResourceTreeIndents);

    initInstantSearch();

    setHelpTopic(this, Qn::MainWindow_Tree_Help);

    m_connections << connect(treeWidget(), &QnResourceTreeWidget::activated,
        m_interactionHandler.get(), &ResourceTreeInteractionHandler::activateItem);

    m_connections << connect(
        treeWidget()->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnResourceBrowserWidget::selectionChanged);

    /* Connect to context. */
    treeWidget()->setWorkbench(workbench());

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
    installEventHandler({this}, {QEvent::Resize, QEvent::Move},
        this, &QnResourceBrowserWidget::geometryChanged);

    m_connections << connect(action(action::SelectAllAction), &QAction::triggered,
        this, &QnResourceBrowserWidget::onSelectAllActionTriggered);

    initToolTip();

    /* Run handlers. */
    updateIcons();

    at_workbench_currentLayoutChanged();
}

QnResourceBrowserWidget::~QnResourceBrowserWidget()
{
    disconnect(workbench(), nullptr, this, nullptr);

    at_workbench_currentLayoutAboutToBeChanged();

    setTooltipResource(QnResourcePtr());
    treeWidget()->setWorkbench(nullptr);
}

int QnResourceBrowserWidget::x() const
{
    return pos().x();
}

void QnResourceBrowserWidget::setX(int x)
{
    QPoint pos_ = pos();

    if (pos_.x() != x)
    {
        move(x, pos_.y());
        emit xChanged(x);
    }
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

void QnResourceBrowserWidget::initToolTip()
{
    // To keep aspect ratio specify only maximum height for server request.
    m_thumbnailManager->setThumbnailSize(QSize(0, kMaxThumbnailSize.height()));

    m_tooltip->setMaxThumbnailSize(kMaxThumbnailSize);
    m_tooltip->setImageProvider(m_thumbnailManager.data());
    m_tooltip->setOpacity(0);
    m_tooltip->setVisible(false);

    connect(m_tooltip, &DeprecatedThumbnailTooltip::clicked, this,
        [this]
        {
            if (!m_tooltip || !m_tooltipResource)
                return;

            menu()->trigger(action::OpenInCurrentLayoutAction, m_tooltipResource);
        });

    m_hoverProcessor->addTargetWidget(this);
    m_hoverProcessor->addTargetWidget(m_tooltip);
    m_hoverProcessor->setHoverEnterDelay(250);
    m_hoverProcessor->setHoverLeaveDelay(250);

    m_connections << QObject::connect(m_hoverProcessor.data(), &HoverFocusProcessor::hoverLeft,
        this, &QnResourceBrowserWidget::hideToolTip);

    m_opacityAnimator->setTimeLimit(200);
    connect(m_opacityAnimator, &VariantAnimator::valueChanged,
        [this](const QVariant& val)
        {
            m_tooltip->setVisible(!qFuzzyIsNull(val.toReal()));
        });

    treeView()->viewport()->installEventFilter(this);
}

void QnResourceBrowserWidget::initInstantSearch()
{
    const auto filterEdit = ui->instantFilterLineEdit;

    setPaletteColor(ui->nothingFoundLabel, QPalette::Text, colorTheme()->color("dark14"));
    setPaletteColor(ui->nothingFoundDescriptionLabel, QPalette::Text, colorTheme()->color("dark14"));

    // Initialize new filter edit.

    connect(filterEdit, &SearchEdit::textChanged,
        this, &QnResourceBrowserWidget::updateInstantFilter);
    connect(filterEdit, &SearchEdit::editingFinished,
        this, &QnResourceBrowserWidget::updateInstantFilter);
    connect(filterEdit, &SearchEdit::currentTagDataChanged,
        this, &QnResourceBrowserWidget::updateInstantFilter);

    connect(commonModule(), &QnCommonModule::remoteIdChanged,
        this, &QnResourceBrowserWidget::updateSearchMode);

    const auto searchModel = treeWidget()->searchModel();

    connect(searchModel, &QAbstractItemModel::rowsInserted,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);
    connect(searchModel, &QAbstractItemModel::rowsRemoved,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);
    connect(searchModel, &QAbstractItemModel::modelReset,
        this, &QnResourceBrowserWidget::handleInstantFilterUpdated);

    auto filterMenuCreator = [this](QWidget* parent) { return createFilterMenu(parent); };
    auto filterNameProvider = [this](const QVariant& data)
        { return getFilterName(data.value<ResourceTree::NodeType>()); };
    filterEdit->setTagOptionsSource(filterMenuCreator, filterNameProvider);

    updateSearchMode();
    handleInstantFilterUpdated();
}

void QnResourceBrowserWidget::updateSearchMode()
{
    const auto filterEdit = ui->instantFilterLineEdit;
    const bool localResourcesMode = commonModule()->remoteGUID().isNull();

    filterEdit->setText(QString());
    if (!localResourcesMode)
    {
        filterEdit->setMenuEnabled(true);
        filterEdit->setPlaceholderText(tr("Search"));
    }
    else
    {
        filterEdit->setMenuEnabled(false);
        filterEdit->setCurrentTagData({});
        filterEdit->setPlaceholderText(tr("Local files"));
    }

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
    using FilterMode = entity_resource_tree::ResourceTreeComposer::FilterMode;
    if (m_resourceTreeComposer->filterMode() != FilterMode::noFilter)
        return;

    if (!m_expandedStatesList.isEmpty())
    {
        NX_ASSERT(false, "Expanded states list should be empty.");
        m_expandedStatesList.clear();
    }

    const auto tree = treeView();
    const auto searchModel = treeWidget()->searchModel();
    const auto callback =
        [this, tree, searchModel](const QModelIndex& index, bool hasChildren)
        {
            if (!hasChildren)
                return;

            const auto searchIndex = searchModel->mapFromSource(index);
            m_expandedStatesList.append(ExpandedState(index, tree->isExpanded(searchIndex)));
        };

    forEachIndex(treeWidget()->model(), QModelIndex(), callback);
}

void QnResourceBrowserWidget::restoreExpandedStates()
{
    const auto searchModel = treeWidget()->searchModel();
    for (const auto& data: m_expandedStatesList)
    {
        const auto& index = data.first;
        const auto searchIndex = searchModel->mapFromSource(index);
        if (searchIndex.isValid())
            treeView()->setExpanded(searchIndex, data.second);
    }
    m_expandedStatesList.clear();
}

void QnResourceBrowserWidget::setupAutoExpandPolicy()
{
    treeWidget()->setAutoExpandPolicy(
        [this](const QModelIndex& index)
        {
            switch (index.data(Qn::NodeTypeRole).value<NodeType>())
            {
                case NodeType::resource:
                {
                    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
                    if (resource && resource->hasFlags(Qn::server))
                    {
                        const auto serverCount =
                            resourcePool()->getResources<QnMediaServerResource>().count();
                        return serverCount <= kMaxAutoExpandedServers;
                    }
                    return false;
                }
                case NodeType::servers:
                case NodeType::camerasAndDevices:
                case NodeType::layouts:
                case NodeType::users:
                    return true;
                default:
                    break;
            }
            return false;
        }
    );
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
        [localResourcesMode, filterEdit]()
        {
            if (!filterEdit->currentTagData().isNull())
                return filterEdit->currentTagData().value<ResourceTree::NodeType>();

            return localResourcesMode
                ? QnResourceSearchQuery::NodeType::localResources
                : QnResourceSearchQuery::kAllowAllNodeTypes;
        }();

    const bool filtering = !trimmed.isEmpty()
        || (!localResourcesMode && !filterEdit->currentTagData().isNull());
    const bool filteringUpdated = updateFilteringMode(filtering);
    if (filteringUpdated && filtering)
        storeExpandedStates();

    using FilterMode = entity_resource_tree::ResourceTreeComposer::FilterMode;

    if (allowedNode == NodeType::camerasAndDevices)
        m_resourceTreeComposer->setFilterMode(FilterMode::allDevicesFilter);
    else if (allowedNode == NodeType::layouts)
        m_resourceTreeComposer->setFilterMode(FilterMode::allLayoutsFilter);
    else
        m_resourceTreeComposer->setFilterMode(FilterMode::noFilter);

    auto query = QnResourceSearchQuery(trimmed, allowedNode);
    const auto searchModel = treeWidget()->searchModel();
    const auto newRootNode = searchModel->setQuery(query);

    treeView()->setRootIndex(newRootNode);
    if (filtering)
        treeView()->expandAll();

    if (filteringUpdated && !filtering)
        restoreExpandedStates();

    handleInstantFilterUpdated();
}

void QnResourceBrowserWidget::handleInstantFilterUpdated()
{
    const auto searchModel = treeWidget()->searchModel();
    const auto rootIndex = treeView()->rootIndex();
    const bool emptyResults = searchModel->rowCount(rootIndex) == 0;
    const bool noLocalFiles = commonModule()->remoteGUID().isNull()
        && searchModel->query().text.trimmed().isEmpty()
        && emptyResults;
    ui->nothingFoundDescriptionLabel->setVisible(noLocalFiles);
    ui->nothingFoundLabel->setText(noLocalFiles ? tr("No local files") : tr("Nothing found"));

    static constexpr int kResourcesPage = 0;
    static constexpr int kNothingFoundPage = 1;
    ui->resourcesHolder->setCurrentIndex(emptyResults ? kNothingFoundPage : kResourcesPage);

    treeView()->setRootIsDecorated(hasParentNodes(treeView()->rootIndex()));
    const auto indents = treeView()->rootIsDecorated()
        ? kStandardResourceTreeIndents
        : kTaggedResourceTreeIndents;
    treeView()->setProperty(nx::style::Properties::kSideIndentation, indents);
}

void QnResourceBrowserWidget::updateTreeItem(const QnWorkbenchItem* item)
{
    if (item && NX_ASSERT(item->resource()))
        treeWidget()->update(item->resource());
}

QMenu* QnResourceBrowserWidget::createFilterMenu(QWidget* parent) const
{
    QMenu* result = new QMenu(parent);

    auto escapeActionText =
        [](const QString& text) { return QString(text).replace(lit("&"), lit("&&")); };

    auto addMenuItem =
        [this, result, escapeActionText](ResourceTree::NodeType filterNodeType)
        {
            auto action = result->addAction(escapeActionText(getFilterName(filterNodeType)));
            action->setData(QVariant::fromValue(filterNodeType));
        };

    const bool showServersInResourceTree =
        accessController()->hasGlobalPermission(GlobalPermission::admin)
        || commonModule()->globalSettings()->showServersInTreeForNonAdmins();

    QList<ResourceTree::NodeType> filterOptions;
    if (showServersInResourceTree)
        filterOptions.append(ResourceTree::NodeType::servers);
    filterOptions.append({
        ResourceTree::NodeType::camerasAndDevices,
        ResourceTree::NodeType::layouts,
        ResourceTree::NodeType::layoutTours,
        ResourceTree::NodeType::videoWalls,
        ResourceTree::NodeType::webPages,
        ResourceTree::NodeType::users,
        ResourceTree::NodeType::localResources});

    addMenuItem(QnResourceSearchQuery::kAllowAllNodeTypes);
    result->addSeparator();
    for (auto filterNodeType: std::as_const(filterOptions))
        addMenuItem(filterNodeType);

    return result;
}

QString QnResourceBrowserWidget::getFilterName(ResourceTree::NodeType allowedNodeType) const
{
    switch (allowedNodeType)
    {
        case QnResourceSearchQuery::kAllowAllNodeTypes:
            return tr("All types");
        case ResourceTree::NodeType::servers:
            return tr("Servers");
        case ResourceTree::NodeType::camerasAndDevices:
            return tr("Cameras & Devices");
        case ResourceTree::NodeType::layouts:
            return tr("Layouts");
        case ResourceTree::NodeType::layoutTours:
            return tr("Showreels");
        case ResourceTree::NodeType::videoWalls:
            return tr("Video Walls");
        case ResourceTree::NodeType::webPages:
            return tr("Web Pages");
        case ResourceTree::NodeType::users:
            return tr("Users");
        case ResourceTree::NodeType::localResources:
            return tr("Local Files");
        default:
            NX_ASSERT(false);
            break;
    }
    return QString();
}

void QnResourceBrowserWidget::showContextMenuAt(const QPoint& pos, bool ignoreSelection)
{
    if (ignoreSelection)
    {
        m_interactionHandler->showContextMenu(mainWindowWidget(), pos, {}, {});
    }
    else
    {
        const auto selectionModel = treeWidget()->selectionModel();
        m_interactionHandler->showContextMenu(mainWindowWidget(),
            pos,
            selectionModel->currentIndex(),
            selectionModel->selectedIndexes());
    }
}

QnResourceTreeWidget* QnResourceBrowserWidget::treeWidget() const
{
    return ui->resourceTreeWidget;
}

QTreeView* QnResourceBrowserWidget::treeView() const
{
    return ui->resourceTreeWidget->treeView();
}

QModelIndex QnResourceBrowserWidget::itemIndexAt(const QPoint& pos) const
{
    if (!treeView()->model())
        return QModelIndex();
    QPoint childPos = treeView()->mapFrom(const_cast<QnResourceBrowserWidget*>(this), pos);
    return treeView()->indexAt(childPos);
}

action::ActionScope QnResourceBrowserWidget::currentScope() const
{
    return action::TreeScope;
}

void QnResourceBrowserWidget::showOwnTooltip(const QPointF& pos)
{
    if (!m_tooltip)
        return; //< Default tooltip should not be displayed anyway.

    const QModelIndex index = itemIndexAt(pos.toPoint());
    if (!index.isValid())
    {
        hideToolTip();
        return;
    }

    const QString tooltipText = index.data(Qt::ToolTipRole).toString();
    if (tooltipText.isEmpty())
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
            text = kHtmlLabelNoInfoFormat.arg(tooltipText);
        else if (resource && resource->hasFlags(Qn::user))
            text = kHtmlLabelUserFormat.arg(userDisplayedName(tooltipText)).arg(extraInfo);
        else
            text = kHtmlLabelDefaultFormat.arg(tooltipText).arg(extraInfo);

        m_tooltip->setText(text);

        QPointF graphicScenePos(pos.x(), pos.y() + geometry().y());
        m_tooltip->pointTo(QPoint(geometry().right(), graphicScenePos.y()));

        setTooltipResource(resource);

        showToolTip();
    }
}

bool QnResourceBrowserWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ToolTip && watched == treeView()->viewport())
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QPoint globalPos = treeView()->viewport()->mapToGlobal(helpEvent->pos());
        showOwnTooltip(mapFromGlobal(globalPos));

        return true;
    }

    return base_type::eventFilter(watched, event);
}

bool QnResourceBrowserWidget::isScrollBarVisible() const
{
    return ui->scrollArea->verticalScrollBar()->isVisible();
}

action::Parameters QnResourceBrowserWidget::currentParameters(action::ActionScope scope) const
{
    if (scope != action::TreeScope)
        return action::Parameters();

    const auto selectionModel = treeWidget()->selectionModel();
    return m_interactionHandler->actionParameters(
        selectionModel->currentIndex(),
        selectionModel->selectedIndexes());
}

void QnResourceBrowserWidget::hideToolTip()
{
    m_opacityAnimator->animateTo(0.0);
}

void QnResourceBrowserWidget::showToolTip()
{
    m_opacityAnimator->animateTo(1.0);
}

void QnResourceBrowserWidget::clearSelection()
{
    treeWidget()->selectionModel()->clear();
}

void QnResourceBrowserWidget::updateIcons()
{
    QnResourceItemDelegate::Options opts = QnResourceItemDelegate::RecordingIcons;
    if (accessController()->hasGlobalPermission(GlobalPermission::editCameras))
        opts |= QnResourceItemDelegate::ProblemIcons;

    treeWidget()->itemDelegate()->setOptions(opts);
}

void QnResourceBrowserWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QWidget* child = childAt(event->pos());
    while (child && child != treeWidget())
        child = child->parentWidget();

    showContextMenuAt(event->globalPos(), !child);
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
        QPoint pos = treeWidget()->selectionPos();
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
}

void QnResourceBrowserWidget::at_workbench_currentLayoutChanged()
{
    auto layout = workbench()->currentLayout();

    /* Bold state has changed. */
    treeWidget()->update();

    connect(layout, &QnWorkbenchLayout::itemAdded,
        this, &QnResourceBrowserWidget::updateTreeItem);
    connect(layout, &QnWorkbenchLayout::itemRemoved,
        this, &QnResourceBrowserWidget::updateTreeItem);
}

void QnResourceBrowserWidget::at_workbench_itemChange(Qn::ItemRole role)
{
    /* Raised state has changed. */
    updateTreeItem(workbench()->item(role));
}

void QnResourceBrowserWidget::setTooltipResource(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    m_tooltipResource = camera;
    m_thumbnailManager->selectCamera(camera);
    m_tooltip->setThumbnailVisible(!camera.isNull());
}

void QnResourceBrowserWidget::selectNodeByUuid(const QnUuid& id)
{
    NX_ASSERT(!id.isNull());
    if (id.isNull())
        return;

    auto model = treeWidget()->searchModel();
    NX_ASSERT(model);
    if (!model)
        return;

    selectIndices(model->match(model->index(0, 0),
        Qn::UuidRole,
        QVariant::fromValue(id),
        1,
        Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive)));
}

void QnResourceBrowserWidget::selectResource(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    auto model = treeWidget()->searchModel();
    NX_ASSERT(model);
    if (!model)
        return;

    selectIndices(model->match(model->index(0, 0),
        Qn::ResourceRole,
        QVariant::fromValue(resource),
        1,
        Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive)));
}

void QnResourceBrowserWidget::selectIndices(const QModelIndexList& indices) const
{
    if (indices.empty())
        return;

    for (const auto& index: indices)
    {
        auto parent = index.parent();
        while (parent.isValid())
        {
            treeView()->expand(parent);
            parent = parent.parent();
        }
    }

    if (auto selection = treeWidget()->selectionModel())
    {
        selection->clear();
        for (const auto& index: indices)
            selection->select(index, QItemSelectionModel::Select);
    }
}

void QnResourceBrowserWidget::onSelectAllActionTriggered()
{
    const auto view = treeView();
    const auto viewModel = view->model();
    const auto viewSelectionModel = view->selectionModel();

    if (!viewModel || !viewSelectionModel)
        return;

    const auto currentIndex = viewSelectionModel->currentIndex();
    const auto model = currentIndex.model();

    if (currentIndex.isValid())
    {
        auto indicesToSelect = model->match(
            currentIndex,
            Qn::NodeTypeRole,
            currentIndex.data(Qn::NodeTypeRole),
            -1,
            {Qt::MatchExactly, Qt::MatchWrap});

        auto indicesToDeselect = viewSelectionModel->selectedIndexes();
        // QList elements are actually erased.
        [[maybe_unused]] auto itr = std::remove_if(
            std::begin(indicesToDeselect), std::end(indicesToDeselect),
            [&currentIndex](const QModelIndex& index)
            {
                return index.parent() != currentIndex.parent();
            });

        for (const auto& index: indicesToDeselect)
            viewSelectionModel->select(index, QItemSelectionModel::Deselect);

        for (const auto& index: indicesToSelect)
            viewSelectionModel->select(index, QItemSelectionModel::Select);
    }
}

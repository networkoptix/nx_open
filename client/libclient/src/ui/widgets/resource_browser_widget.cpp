﻿#include "resource_browser_widget.h"
#include "ui_resource_browser_widget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtCore/QItemSelectionModel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QGraphicsLinearLayout>
#include <camera/camera_thumbnail_manager.h>

#include <client/client_runtime_settings.h>

#include <common/common_meta_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_property.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <nx/client/ui/workbench/workbench_animations.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/models/resource_search_synchronizer.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/text_edit_label.h>
#include <ui/widgets/resource_preview_widget.h>
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

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

const char* kSearchModelPropertyName = "_qn_searchModel";
const char* kSearchSynchronizerPropertyName = "_qn_searchSynchronizer";
const char* kFilterPropertyName = "_qn_filter";

const int kNoDataFontPixelSize = 32;
const int kNoDataFontWeight = QFont::Light;

const auto kHtmlLabelNoInfoFormat = lit("<center><span style='font-weight: 500'>%1</span></center>");
const auto kHtmlLabelDefaultFormat = lit("<center><span style='font-weight: 500'>%1</span> %2</center>");
const auto kHtmlLabelUserFormat = lit("<center><span style='font-weight: 500'>%1</span> &mdash; %2</center>");

const QSize kMaxThumbnailSize(224, 184);

} // namespace

// -------------------------------------------------------------------------- //
// QnResourceBrowserToolTipWidget
// -------------------------------------------------------------------------- //
QnResourceBrowserToolTipWidget::QnResourceBrowserToolTipWidget(QGraphicsItem* parent):
    base_type(parent),
    m_proxyWidget(new Clickable<QGraphicsProxyWidget>(this)),
    m_embeddedWidget(new QWidget()),
    m_textLabel(new QnTextEditLabel(m_embeddedWidget)),
    m_previewWidget(new QnResourcePreviewWidget(m_embeddedWidget))
{
    m_proxyWidget->setVisible(false);
    m_proxyWidget->setWidget(m_embeddedWidget);
    m_proxyWidget->installSceneEventFilter(this);

    m_embeddedWidget->setAttribute(Qt::WA_TranslucentBackground);

    /* To keep aspect ratio specify only maximum height for server request: */
    m_previewWidget->setThumbnailSize(QSize(0, kMaxThumbnailSize.height()));
    /* And specify maximum width and height for the widget: */
    m_previewWidget->setMaximumSize(kMaxThumbnailSize);

    auto dots = m_previewWidget->busyIndicator()->dots();
    dots->setDotRadius(style::Metrics::kStandardPadding / 2.0);
    dots->setDotSpacing(style::Metrics::kStandardPadding);

    auto layout = new QVBoxLayout(m_embeddedWidget);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_previewWidget);
    layout->addWidget(m_textLabel);

    auto graphicsLayout = new QGraphicsLinearLayout(Qt::Vertical);
    graphicsLayout->setContentsMargins(0, 0, 0, 0);
    graphicsLayout->addItem(m_proxyWidget);
    setLayout(graphicsLayout);

    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    m_previewWidget->setFont(font);

    setThumbnailVisible(false);
}

bool QnResourceBrowserToolTipWidget::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
    if (watched == m_proxyWidget
        && event->type() == QEvent::GraphicsSceneMousePress
        && static_cast<QGraphicsSceneMouseEvent*>(event)->button() == Qt::LeftButton)
    {
        thumbnailClicked();
    }

    return base_type::sceneEventFilter(watched, event);
}

void QnResourceBrowserToolTipWidget::forceLayoutUpdate()
{
    m_embeddedWidget->layout()->activate();
}

void QnResourceBrowserToolTipWidget::setText(const QString& text)
{
    m_textLabel->setText(text);
    forceLayoutUpdate();
}

void QnResourceBrowserToolTipWidget::setThumbnailVisible(bool visible)
{
    if (m_previewWidget->isHidden() != visible)
        return;

    m_textLabel->setSizePolicy(
        visible ? QSizePolicy::Ignored : QSizePolicy::Preferred,
        QSizePolicy::Preferred);

    m_textLabel->setWordWrapMode(visible
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::ManualWrap);

    m_previewWidget->setVisible(visible);
    forceLayoutUpdate();

    updateTailPos();
}

void QnResourceBrowserToolTipWidget::setResource(const QnResourcePtr& resource)
{
    if (m_previewWidget->targetResource() == resource)
        return;

    m_previewWidget->setTargetResource(resource);
    forceLayoutUpdate();
}

const QnResourcePtr& QnResourceBrowserToolTipWidget::resource() const
{
    return m_previewWidget->targetResource();
}

void QnResourceBrowserToolTipWidget::updateTailPos()
{
    QRectF rect = this->rect();
    QGraphicsWidget* parent = parentWidget();
    QRectF enclosingRect = parent->geometry();

    // half of the tooltip height in coordinates of enclosing rect
    qreal halfHeight = mapRectToItem(parent, rect).height() / 2;

    qreal parentPos = m_pointTo.y();

    if (parentPos - halfHeight < 0)
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound(rect.top() + tailWidth())));
    else if (parentPos + halfHeight > enclosingRect.height())
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound(rect.bottom() - tailWidth())));
    else
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound((rect.top() + rect.bottom()) / 2)));

    base_type::pointTo(m_pointTo);
}

void QnResourceBrowserToolTipWidget::pointTo(const QPointF& pos)
{
    m_pointTo = pos;
    base_type::pointTo(pos);
    updateTailPos();
}

// -------------------------------------------------------------------------- //
// QnResourceBrowserWidget
// -------------------------------------------------------------------------- //
QnResourceBrowserWidget::QnResourceBrowserWidget(QWidget* parent, QnWorkbenchContext* context):
    QWidget(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::ResourceBrowserWidget()),
    m_ignoreFilterChanges(false),
    m_filterTimerId(0),
    m_tooltipWidget(nullptr),
    m_hoverProcessor(nullptr)
{
    ui->setupUi(this);

    ui->typeComboBox->addItem(tr("Any Type"), 0);
    ui->typeComboBox->addItem(tr("Video Files"), static_cast<int>(Qn::local | Qn::video));
    ui->typeComboBox->addItem(tr("Image Files"), static_cast<int>(Qn::still_image));
    ui->typeComboBox->addItem(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Live Devices"),
        tr("Live Cameras")
    ), static_cast<int>(Qn::live));

    m_resourceModel = new QnResourceTreeModel(QnResourceTreeModel::FullScope, this);
    ui->resourceTreeWidget->setModel(m_resourceModel);
    ui->resourceTreeWidget->setCheckboxesVisible(false);
    ui->resourceTreeWidget->setGraphicsTweaks(Qn::HideLastRow | Qn::BypassGraphicsProxy);
    ui->resourceTreeWidget->setEditingEnabled();
    //    ui->resourceTreeWidget->setFilterVisible(); //TODO: #Elric why don't we enable this? looks good and useful

    ui->searchTreeWidget->setCheckboxesVisible(false);

    /* This is needed so that control's context menu is not embedded into the scene. */
    ui->filterLineEdit->setWindowFlags(ui->filterLineEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);

    m_renameActions.insert(QnActions::RenameResourceAction, new QAction(this));
    m_renameActions.insert(QnActions::RenameVideowallEntityAction, new QAction(this));

    setHelpTopic(this, Qn::MainWindow_Tree_Help);
    setHelpTopic(ui->searchTab, Qn::MainWindow_Tree_Search_Help);

    connect(ui->typeComboBox, QnComboboxCurrentIndexChanged,
        this, [this]() { updateFilter(false); });
    connect(ui->filterLineEdit, &QnSearchLineEdit::textChanged,
        this, [this]() { updateFilter(false); });
    connect(ui->filterLineEdit->lineEdit(), &QLineEdit::editingFinished,
        this, [this]() { updateFilter(true); });

    connect(ui->resourceTreeWidget, &QnResourceTreeWidget::activated,
        this, &QnResourceBrowserWidget::handleItemActivated);
    connect(ui->searchTreeWidget, &QnResourceTreeWidget::activated,
        this, &QnResourceBrowserWidget::handleItemActivated);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
        this, &QnResourceBrowserWidget::at_tabWidget_currentChanged);
    connect(ui->resourceTreeWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnResourceBrowserWidget::selectionChanged);

    /* Connect to context. */
    ui->resourceTreeWidget->setWorkbench(workbench());
    ui->searchTreeWidget->setWorkbench(workbench());

    setTabShape(ui->tabWidget->tabBar(), style::TabShape::Compact);
    ui->tabWidget->setProperty(style::Properties::kTabBarIndent, style::Metrics::kDefaultTopLevelMargin);
    ui->tabWidget->tabBar()->setMaximumHeight(32);

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged,
        this, &QnResourceBrowserWidget::at_workbench_currentLayoutAboutToBeChanged);
    connect(workbench(), &QnWorkbench::currentLayoutChanged,
        this, &QnResourceBrowserWidget::at_workbench_currentLayoutChanged);
    connect(workbench(), &QnWorkbench::itemChanged,
        this, &QnResourceBrowserWidget::at_workbench_itemChanged);

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged,
        this, &QnResourceBrowserWidget::updateIcons);

    connect(this->context(), &QnWorkbenchContext::userChanged,
        this, [this]() { ui->tabWidget->setCurrentWidget(ui->resourcesTab); });

    installEventHandler({ ui->resourceTreeWidget->treeView()->verticalScrollBar(),
        ui->searchTreeWidget->treeView()->verticalScrollBar() }, { QEvent::Show, QEvent::Hide },
        this, &QnResourceBrowserWidget::scrollBarVisibleChanged);

    /* Run handlers. */
    updateFilter();
    updateIcons();

    at_workbench_currentLayoutChanged();
}

QnResourceBrowserWidget::~QnResourceBrowserWidget()
{
    disconnect(workbench(), nullptr, this, nullptr);

    at_workbench_currentLayoutAboutToBeChanged();

    ui->searchTreeWidget->setWorkbench(nullptr);
    ui->resourceTreeWidget->setWorkbench(nullptr);

    /* Workaround against #3797 */
    ui->typeComboBox->setEnabled(false);
}

QComboBox* QnResourceBrowserWidget::typeComboBox() const
{
    return ui->typeComboBox;
}

QnResourceBrowserWidget::Tab QnResourceBrowserWidget::currentTab() const
{
    return static_cast<Tab>(ui->tabWidget->currentIndex());
}

void QnResourceBrowserWidget::setCurrentTab(Tab tab)
{
    if (tab < 0 || tab >= TabCount)
    {
        qnWarning("Invalid resource tree widget tab '%1'.", static_cast<int>(tab));
        return;
    }

    ui->tabWidget->setCurrentIndex(tab);
}

bool QnResourceBrowserWidget::isLayoutSearchable(QnWorkbenchLayout* layout) const
{
    return accessController()->hasPermissions(layout->resource(), Qn::WritePermission | Qn::AddRemoveItemsPermission);
}

QnResourceSearchProxyModel* QnResourceBrowserWidget::layoutModel(QnWorkbenchLayout* layout, bool create) const
{
    QnResourceSearchProxyModel* result = layout->property(kSearchModelPropertyName).value<QnResourceSearchProxyModel*>();
    if (create && !result)
    {
        result = new QnResourceSearchProxyModel(layout);
        result->setFilterCaseSensitivity(Qt::CaseInsensitive);
        result->setFilterKeyColumn(0);
        result->setFilterRole(Qn::ResourceSearchStringRole);
        result->setSortCaseSensitivity(Qt::CaseInsensitive);
        result->setDynamicSortFilter(true);
        result->setSourceModel(m_resourceModel);
        layout->setProperty(kSearchModelPropertyName, QVariant::fromValue<QnResourceSearchProxyModel*>(result));

        //initial filter setup
        setupInitialModelCriteria(result);
    }
    return result;
}

QnResourceSearchSynchronizer* QnResourceBrowserWidget::layoutSynchronizer(QnWorkbenchLayout* layout, bool create) const
{
    QnResourceSearchSynchronizer* result = layout->property(kSearchSynchronizerPropertyName).value<QnResourceSearchSynchronizer*>();
    if (create && !result && isLayoutSearchable(layout))
    {
        result = new QnResourceSearchSynchronizer(layout);
        result->setLayout(layout);
        result->setModel(layoutModel(layout, true));
        layout->setProperty(kSearchSynchronizerPropertyName, QVariant::fromValue<QnResourceSearchSynchronizer*>(result));
    }
    return result;
}

QString QnResourceBrowserWidget::layoutFilter(QnWorkbenchLayout* layout) const
{
    return layout->property(kFilterPropertyName).toString();
}

void QnResourceBrowserWidget::setLayoutFilter(QnWorkbenchLayout* layout, const QString& filter) const
{
    layout->setProperty(kFilterPropertyName, filter);
}

void QnResourceBrowserWidget::killSearchTimer()
{
    if (m_filterTimerId == 0)
        return;

    killTimer(m_filterTimerId);
    m_filterTimerId = 0;
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

    QnActionManager* manager = context()->menu();

    QScopedPointer<QMenu> menu(manager->newMenu(Qn::TreeScope, nullptr, ignoreSelection
        ? QnActionParameters().withArgument(Qn::NodeTypeRole, Qn::RootNode)
        : currentParameters(Qn::TreeScope)));

    if (currentTreeWidget() == ui->searchTreeWidget)
    {
        /* Disable rename action for search view. */
        for (QnActions::IDType key : m_renameActions.keys())
            manager->redirectAction(menu.data(), key, nullptr);
    }
    else
    {
        for (QnActions::IDType key : m_renameActions.keys())
            manager->redirectAction(menu.data(), key, m_renameActions[key]);
    }

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
    if (ui->tabWidget->currentIndex() == ResourcesTab)
    {
        return ui->resourceTreeWidget;
    }
    else
    {
        return ui->searchTreeWidget;
    }
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
        Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

        switch (nodeType)
        {
            case Qn::RecorderNode:
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
            case Qn::ResourceNode:
            case Qn::SharedLayoutNode:
            case Qn::SharedResourceNode:
            case Qn::EdgeNode:
            case Qn::CurrentUserNode:
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
        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(uuid);
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

        QnVideoWallMatrixIndex index = qnResPool->getVideoWallMatrixByUuid(uuid);
        if (!index.isNull())
            result.push_back(index);
    }

    return result;
}

Qn::ActionScope QnResourceBrowserWidget::currentScope() const
{
    return Qn::TreeScope;
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

    QModelIndex index = itemIndexAt(pos.toPoint());
    if (!index.isValid())
        return true;

    QVariant toolTip = index.data(Qt::ToolTipRole);
    QString toolTipText = toolTip.convert(QVariant::String) ? toolTip.toString() : QString();

    if (toolTipText.isEmpty())
    {
        hideToolTip();
    }
    else
    {
        auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        auto extraInfo = QnResourceDisplayInfo(resource).extraInfo();

        QString text;
        if (extraInfo.isEmpty())
            text = kHtmlLabelNoInfoFormat.arg(toolTipText);
        else if (resource && resource->hasFlags(Qn::user))
            text = kHtmlLabelUserFormat.arg(toolTipText).arg(extraInfo);
        else
            text = kHtmlLabelDefaultFormat.arg(toolTipText).arg(extraInfo);

        m_tooltipWidget->setText(text);
        m_tooltipWidget->pointTo(QPointF(geometry().right(), pos.y()));

        auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        m_tooltipWidget->setResource(camera);
        m_tooltipWidget->setThumbnailVisible(camera != nullptr);

        showToolTip();
    }

    return true;
}

void QnResourceBrowserWidget::setToolTipParent(QGraphicsWidget* widget)
{
    if (m_tooltipWidget)
        return;

    m_tooltipWidget = new QnResourceBrowserToolTipWidget(widget);
    m_hoverProcessor = new HoverFocusProcessor(widget);

    m_tooltipWidget->setFocusProxy(widget);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);

    //    m_tooltipWidget->installEventFilter(item);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget, SIGNAL(thumbnailClicked()), this, SLOT(at_thumbnailClicked()));
    connect(m_tooltipWidget, SIGNAL(tailPosChanged()), this, SLOT(updateToolTipPosition()));
    connect(widget, SIGNAL(geometryChanged()), this, SLOT(updateToolTipPosition()));

    m_hoverProcessor->addTargetItem(widget);
    m_hoverProcessor->addTargetItem(m_tooltipWidget);
    m_hoverProcessor->setHoverEnterDelay(250);
    m_hoverProcessor->setHoverLeaveDelay(250);
    connect(m_hoverProcessor, SIGNAL(hoverLeft()), this, SLOT(hideToolTip()));

    updateToolTipPosition();
}

bool QnResourceBrowserWidget::isScrollBarVisible() const
{
    return currentTreeWidget()->treeView()->verticalScrollBar()->isVisible();
}

QnActionParameters QnResourceBrowserWidget::currentParameters(Qn::ActionScope scope) const
{
    if (scope != Qn::TreeScope)
        return QnActionParameters();

    QItemSelectionModel* selectionModel = currentSelectionModel();
    QModelIndex index = selectionModel->currentIndex();

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    auto withNodeType = [nodeType](QnActionParameters parameters)
        {
            return parameters.withArgument(Qn::NodeTypeRole, nodeType);
        };

    switch (nodeType)
    {
        case Qn::VideoWallItemNode:
            return withNodeType(selectedVideoWallItems());
        case Qn::VideoWallMatrixNode:
            return withNodeType(selectedVideoWallMatrices());
        case Qn::CloudSystemNode:
        {
            QnActionParameters result;
            result.setArgument(Qn::CloudSystemIdRole, index.data(Qn::CloudSystemIdRole).toString());
            return withNodeType(result);
        }
        default:
            break;
    }

    if (!index.data(Qn::ItemUuidRole).value<QnUuid>().isNull()) /* If it's a layout item. */
        return withNodeType(selectedLayoutItems());

    QnActionParameters result(selectedResources());

    /* For working with shared layout links we must know owning user resource. */
    QModelIndex parentIndex = index.parent();
    Qn::NodeType parentNodeType = parentIndex.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    /* We can select several layouts and some other resources in any part of tree - in this case just do not set anything. */
    QnUserResourcePtr user;
    QnUuid roleId;

    if (nodeType == Qn::RoleNode)
        roleId = index.data(Qn::UuidRole).value<QnUuid>();

    switch (parentNodeType)
    {
        case Qn::LayoutsNode:
            user = context()->user();
            break;
        case Qn::SharedResourcesNode:
        case Qn::SharedLayoutsNode:
            user = parentIndex.parent().data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
            roleId = parentIndex.parent().data(Qn::UuidRole).value<QnUuid>();
            break;
        case Qn::ResourceNode:
            user = parentIndex.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
            break;
        default:
            break;
    }

    result.setArgument(Qn::UserResourceRole, user);
    result.setArgument(Qn::UuidRole, roleId);
    result.setArgument(Qn::NodeTypeRole, nodeType);
    return result;
}

void QnResourceBrowserWidget::updateFilter(bool force)
{
    QString filter = ui->filterLineEdit->text();

    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty())
    {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    killSearchTimer();

    if (!workbench())
        return;

    if (m_ignoreFilterChanges)
        return;

    if (!force)
    {
        //        int pos = qMax(filter.lastIndexOf(QLatin1Char('+')), filter.lastIndexOf(QLatin1Char('\\'))) + 1;

        int pos = 0;
        /* Estimate size of the each term in filter expression. */
        while (pos < filter.size())
        {
            int size = 0;
            for (; pos < filter.size(); pos++)
            {
                if (filter[pos] == QLatin1Char('+') || filter[pos] == QLatin1Char('\\'))
                    break;

                if (!filter[pos].isSpace())
                    size++;
            }
            pos++;
            if (size > 0 && size < 3)
                return; /* Filter too short, ignore. */
        }
    }

    m_filterTimerId = startTimer(filter.isEmpty() ? 0 : 300);
}


void QnResourceBrowserWidget::hideToolTip()
{
    if (!m_tooltipWidget)
        return;

    using namespace nx::client::ui::workbench;

    auto animator = opacityAnimator(m_tooltipWidget);
    qnWorkbenchAnimations->setupAnimator(animator, Animations::Id::ResourcesPanelTooltipHide);
    animator->animateTo(0.0);
}

void QnResourceBrowserWidget::showToolTip()
{
    if (!m_tooltipWidget)
        return;

    using namespace nx::client::ui::workbench;

    auto animator = opacityAnimator(m_tooltipWidget);
    qnWorkbenchAnimations->setupAnimator(animator, Animations::Id::ResourcesPanelTooltipShow);
    animator->animateTo(1.0);
}

void QnResourceBrowserWidget::updateIcons()
{
    QnResourceItemDelegate::Options opts = QnResourceItemDelegate::RecordingIcons;
    if (accessController()->hasGlobalPermission(Qn::GlobalEditCamerasPermission))
        opts |= QnResourceItemDelegate::ProblemIcons;

    ui->resourceTreeWidget->itemDelegate()->setOptions(opts);
    ui->searchTreeWidget->itemDelegate()->setOptions(opts);
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
    while (child && child != ui->resourceTreeWidget && child != ui->searchTreeWidget)
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
        if (ui->filterLineEdit->hasFocus())
            return;

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

void QnResourceBrowserWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_filterTimerId)
    {
        killTimer(m_filterTimerId);
        m_filterTimerId = 0;

        if (workbench())
        {
            QnWorkbenchLayout* layout = workbench()->currentLayout();
            QnResourceSearchProxyModel* model = layoutModel(layout, true);

            QString filter = ui->filterLineEdit->text();
            Qn::ResourceFlags flags = static_cast<Qn::ResourceFlags>(ui->typeComboBox->itemData(ui->typeComboBox->currentIndex()).toInt());

            model->clearCriteria();
            model->setFilterWildcard(L'*' + filter + L'*');
            if (filter.isEmpty())
            {
                model->addCriterion(QnResourceCriterionGroup(QnResourceCriterion::Reject, QnResourceCriterion::Reject));
            }
            else
            {
                model->addCriterion(QnResourceCriterionGroup(filter));
                model->addCriterion(QnResourceCriterion(Qn::user));
                model->addCriterion(QnResourceCriterion(Qn::layout));
            }
            if (flags != 0)
                model->addCriterion(QnResourceCriterion(flags, QnResourceProperty::flags, QnResourceCriterion::Next, QnResourceCriterion::Reject));

            setupInitialModelCriteria(model);
        }
    }

    QWidget::timerEvent(event);
}

void QnResourceBrowserWidget::paintEvent(QPaintEvent* event)
{
    /* Underly alternative background for when search tab is selected: */
    if (ui->tabWidget->currentIndex() == SearchTab)
    {
        QPainter painter(this);
        QRect rectToFill(rect());
        rectToFill.adjust(1, 1, -1, -1);
        // rectToFill.setBottom(ui->horizontalLine->mapTo(this, QPoint(0, 0)).y()); //TODO #vkutin this line might be needed later. Remove it if not.
        painter.fillRect(rectToFill, palette().alternateBase());
    }

    QWidget::paintEvent(event);
}

void QnResourceBrowserWidget::at_workbench_currentLayoutAboutToBeChanged()
{
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    disconnect(layout, nullptr, this, nullptr);

    QnResourceSearchSynchronizer* synchronizer = layoutSynchronizer(layout, false);
    if (synchronizer)
        synchronizer->disableUpdates();
    setLayoutFilter(layout, ui->filterLineEdit->text());

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreFilterChanges, true);
    ui->searchTreeWidget->setModel(nullptr);
    ui->filterLineEdit->lineEdit()->setText(QString());
    killSearchTimer();
}

void QnResourceBrowserWidget::at_workbench_currentLayoutChanged()
{
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    QnResourceSearchSynchronizer* synchronizer = layoutSynchronizer(layout, false);
    if (synchronizer)
        synchronizer->enableUpdates();

    at_tabWidget_currentChanged(ui->tabWidget->currentIndex());

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreFilterChanges, true);
    ui->filterLineEdit->lineEdit()->setText(layoutFilter(layout));

    /* Bold state has changed. */
    currentTreeWidget()->update();

    connect(layout, SIGNAL(itemAdded(QnWorkbenchItem*)), this, SLOT(at_layout_itemAdded(QnWorkbenchItem*)));
    connect(layout, SIGNAL(itemRemoved(QnWorkbenchItem*)), this, SLOT(at_layout_itemRemoved(QnWorkbenchItem*)));
}

void QnResourceBrowserWidget::at_workbench_itemChanged(Qn::ItemRole /*role*/)
{
    /* Raised state has changed. */
    if (ui->tabWidget->currentWidget() == ui->resourcesTab)
        ui->resourceTreeWidget->update();
    else
        ui->searchTreeWidget->update();
}

void QnResourceBrowserWidget::at_layout_itemAdded(QnWorkbenchItem*)
{
    /* Bold state has changed. */
    currentTreeWidget()->update();
}

void QnResourceBrowserWidget::at_layout_itemRemoved(QnWorkbenchItem*)
{
    /* Bold state has changed. */
    currentTreeWidget()->update();
}

void QnResourceBrowserWidget::at_tabWidget_currentChanged(int index)
{
    if (index == SearchTab)
    {
        QnWorkbenchLayout* layout = workbench()->currentLayout();
        if (!layout || !layout->resource())
            return;

        layoutSynchronizer(layout, true); /* Just initialize the synchronizer. */
        QnResourceSearchProxyModel* model = layoutModel(layout, true);

        ui->searchTreeWidget->setModel(model);
        ui->searchTreeWidget->expandAll();

        /* View re-creates selection model for each model that is supplied to it,
         * so we have to re-connect each time the model changes. */
        connect(ui->searchTreeWidget->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SIGNAL(selectionChanged()), Qt::UniqueConnection);

        ui->filterLineEdit->setFocus();
    }

    emit currentTabChanged();
    emit scrollBarVisibleChanged();
}

void QnResourceBrowserWidget::at_thumbnailClicked()
{
    if (!m_tooltipWidget || !m_tooltipWidget->resource())
        return;
    if (!m_tooltipWidget->resource()->resourcePool())
        return;
    menu()->trigger(QnActions::OpenInCurrentLayoutAction, QnActionParameters(m_tooltipWidget->resource()));
}

void QnResourceBrowserWidget::setupInitialModelCriteria(QnResourceSearchProxyModel* model) const
{
    /* Always accept servers for administrator users. */
    if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
    {
        model->addCriterion(QnResourceCriterion(Qn::server));
    }
    else
    {
        /* Always skip servers for common users, but always show user and layouts */
        model->addCriterion(QnResourceCriterion(Qn::server, QnResourceProperty::flags, QnResourceCriterion::Reject, QnResourceCriterion::Next));
        model->addCriterion(QnResourceCriterion(Qn::user));
        model->addCriterion(QnResourceCriterion(Qn::layout));
    }
}

void QnResourceBrowserWidget::handleItemActivated(const QModelIndex& index, bool withMouse)
{
    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    if (nodeType == Qn::CloudSystemNode)
    {
        menu()->trigger(QnActions::ConnectToCloudSystemAction, QnActionParameters()
            .withArgument(Qn::CloudSystemIdRole, index.data(Qn::CloudSystemIdRole).toString()));
        return;
    }

    if (nodeType == Qn::VideoWallItemNode)
    {
        auto item = qnResPool->getVideoWallItemByUuid(index.data(Qn::UuidRole).value<QnUuid>());
        menu()->triggerIfPossible(QnActions::StartVideoWallControlAction,
            QnVideoWallItemIndexList() << item);
        return;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    /* Do not open users or fake servers. */
    if (!resource || resource->hasFlags(Qn::user) || resource->hasFlags(Qn::fake))
        return;

    /* Do not open servers of admin.  */
    if (nodeType == Qn::ResourceNode && resource->hasFlags(Qn::server) && withMouse)
        return;

    menu()->trigger(QnActions::DropResourcesAction, resource);
}

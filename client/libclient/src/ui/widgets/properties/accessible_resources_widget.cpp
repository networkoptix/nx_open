#include "accessible_resources_widget.h"
#include "ui_accessible_resources_widget.h"

#include <client/client_globals.h>
#include <client/client_message_processor.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/event_processors.h>

#include <nx/utils/string.h>


namespace {
const int kNoDataFontPixelSize = 24;
const int kNoDataFontWeight = QFont::Light;
const int kCameraNameFontPixelSize = 13;
const int kCameraNameFontWeight = QFont::DemiBold;
const int kCameraPreviewWidthPixels = 160;

class QnAllCheckableModel: public QnResourceListSortedModel
{
    using base_type = QnResourceListSortedModel;

public:
    explicit QnAllCheckableModel(QObject *parent = 0):
        base_type(parent),
        m_allChecked(false)
    {
    }

    virtual ~QnAllCheckableModel()
    {
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (m_allChecked && role == Qt::CheckStateRole &&
            index.data(Qn::ColumnDataRole) == QnResourceListModel::CheckColumn)
        {
            return QVariant::fromValue<int>(Qt::Checked);
        }

        return base_type::data(index, role);
    }

    bool allChecked() const
    {
        return m_allChecked;
    }

    void setAllChecked(bool value)
    {
        if (m_allChecked == value)
            return;

        m_allChecked = value;

        emit dataChanged(
            index(0, QnResourceListModel::CheckColumn),
            index(rowCount() - 1, QnResourceListModel::CheckColumn),
            {Qt::CheckStateRole});
    }

private:
    bool m_allChecked;
};

const QString kDummyResourceId(lit("dummy_resource"));
}

QnAccessibleResourcesWidget::QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnResourceAccessFilter::Filter filter, QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AccessibleResourcesWidget()),
    m_permissionsModel(permissionsModel),
    m_filter(filter),
    m_controlsVisible(filter == QnResourceAccessFilter::MediaFilter), /*< Show 'All' checkbox only for cameras. */
    m_resourcesModel(new QnResourceListModel()),
    m_controlsModel(new QnResourceListModel())
{
    ui->setupUi(this);

    switch (m_filter)
    {
        case QnResourceAccessFilter::LayoutsFilter:
            ui->descriptionLabel->setText(tr("Giving access to some layouts you give access to all cameras on them. Also user will get access to all new cameras on these layouts."));
            break;
        default:
            break;
    }

    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    ui->previewWidget->setFont(font);
    ui->previewWidget->setThumbnailSize(QSize(kCameraPreviewWidthPixels, 0));
    ui->previewWidget->hide();

    font.setPixelSize(kCameraNameFontPixelSize);
    font.setWeight(kCameraNameFontWeight);
    ui->namePlainText->setFont(font);
    ui->namePlainText->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->namePlainText->setFocusPolicy(Qt::NoFocus);
    ui->namePlainText->viewport()->unsetCursor();
    ui->namePlainText->document()->setDocumentMargin(0.0);
    ui->namePlainText->hide();

    /*
     * We use QPlainTextEdit instead of QLabel because we want the label wrapped at any place, not just word boundary.
     *  But it doesn't have precise vertical size hint, therefore we have to adjust maximum vertical size.
     */
    connect(ui->namePlainText->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this](const QSizeF& size)
        {
            /* QPlainTextDocument measures height in lines, not pixels. */
            int height = static_cast<int>(this->fontMetrics().height() * size.height());
            ui->namePlainText->setMaximumHeight(height);
        });

    initControlsModel();
    initResourcesModel();

    QnAllCheckableModel* viewModel = new QnAllCheckableModel(this);
    viewModel->setSourceModel(m_resourcesModel.data());
    viewModel->sort(QnResourceListModel::NameColumn);

    connect(this, &QnAccessibleResourcesWidget::controlsChanged, viewModel, &QnAllCheckableModel::setAllChecked);

    ui->resourcesTreeView->setModel(viewModel);
    ui->controlsTreeView->setModel(m_controlsModel.data());
    ui->controlsTreeView->setVisible(m_controlsVisible);
    ui->controlsTreeView->setEnabled(m_controlsVisible);
    ui->line->setVisible(m_controlsVisible);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->resourcesListWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->resourcesTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    if (m_controlsVisible)
    {
        auto keyPressSignalizer = new QnSingleEventSignalizer(this);
        keyPressSignalizer->setEventType(QEvent::KeyPress);
        ui->resourcesTreeView->installEventFilter(keyPressSignalizer);
        ui->controlsTreeView->installEventFilter(keyPressSignalizer);
        connect(keyPressSignalizer, &QnSingleEventSignalizer::activated,
            this, &QnAccessibleResourcesWidget::at_itemViewKeyPress);
    }

    auto showHideSignalizer = new QnMultiEventSignalizer(this);
    showHideSignalizer->addEventType(QEvent::Show);
    showHideSignalizer->addEventType(QEvent::Hide);
    scrollBar->installEventFilter(showHideSignalizer);
    connect(showHideSignalizer, &QnMultiEventSignalizer::activated, this,
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
        treeView->setItemDelegate(itemDelegate);
        treeView->header()->setStretchLastSection(false);
        treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        treeView->header()->setSectionResizeMode(QnResourceListModel::NameColumn, QHeaderView::Stretch);
        treeView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(kIndents));
    };
    setupTreeView(ui->resourcesTreeView);
    setupTreeView(ui->controlsTreeView);

    ui->resourcesTreeView->setMouseTracking(true);

    auto toggleCheckbox = [this](const QModelIndex& index)
    {
        QAbstractItemModel* model = static_cast<QnTreeView*>(sender())->model();
        QModelIndex checkboxIdx = index.sibling(index.row(), QnResourceListModel::CheckColumn);
        int newCheckValue = checkboxIdx.data(Qt::CheckStateRole).toInt() != Qt::Checked ? Qt::Checked : Qt::Unchecked;
        model->setData(checkboxIdx, newCheckValue, Qt::CheckStateRole);
    };

    connect(ui->resourcesTreeView, &QnTreeView::clicked, this, toggleCheckbox);
    connect(ui->controlsTreeView, &QnTreeView::clicked, this, toggleCheckbox);

    auto batchToggleCheckboxes = [this](const QModelIndex& index)
    {
        QAbstractItemModel* model = static_cast<QnTreeView*>(sender())->model();
        QModelIndex checkboxIdx = index.sibling(index.row(), QnResourceListModel::CheckColumn);
        QModelIndexList selectedRows = ui->resourcesTreeView->selectionModel()->selectedRows(QnResourceListModel::CheckColumn);
        selectedRows << checkboxIdx;

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
    connect(ui->controlsTreeView, &QnTreeView::spacePressed, this, batchToggleCheckboxes);

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

    return m_resourcesModel->checkedResources() != QnResourceAccessFilter::filteredResources(m_filter, m_permissionsModel->accessibleResources());
}

void QnAccessibleResourcesWidget::loadDataToUi()
{
    if (m_controlsVisible)
    {
        QSet<QnUuid> checkedControls;
        if (m_permissionsModel->rawPermissions().testFlag(Qn::GlobalAccessAllMediaPermission))
            for (const auto& resource : m_controlsModel->resources())
                checkedControls << resource->getId();   /*< Really we are checking the only dummy resource. */
        m_controlsModel->setCheckedResources(checkedControls);
    }
    m_resourcesModel->setCheckedResources(QnResourceAccessFilter::filteredResources(m_filter, m_permissionsModel->accessibleResources()));
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

void QnAccessibleResourcesWidget::applyChanges()
{
    auto accessibleResources = m_permissionsModel->accessibleResources();
    auto oldFiltered = QnResourceAccessFilter::filteredResources(m_filter, accessibleResources);
    auto newFiltered = m_resourcesModel->checkedResources();

    if (m_filter == QnResourceAccessFilter::LayoutsFilter)
    {
        QnLayoutResourceList layoutsToShare = qnResPool->getResources(newFiltered)
            .filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
        {
            return !layout->isShared();
        });
        for (const auto& layout : layoutsToShare)
        {
            layout->setParentId(QnUuid());
            menu()->trigger(QnActions::SaveLayoutAction, QnActionParameters(layout));
        }
    }

    accessibleResources.subtract(oldFiltered);
    accessibleResources.unite(newFiltered);

    /* Some resources may be deleted while we are editing user. Do not store them in DB. */
    QSet<QnUuid> unavailable;
    for (const QnUuid& id : accessibleResources)
    {
        if (!qnResPool->getResourceById(id))
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
    dummy->setName(tr("All Media Resources"));
    /* Create separate dummy resource id for each filter, but once per application run. */
    dummy->setId(QnUuid::createUuidFromPool(guidFromArbitraryData(kDummyResourceId).getQUuid(), m_filter));
    qnResIconCache->setKey(dummy, QnResourceIconCache::Cameras);
    m_controlsModel->setResources(QnResourceList() << dummy);
    m_controlsModel->setHasCheckboxes(true);
    m_controlsModel->setUserCheckable(false);
    m_controlsModel->setStatusIgnored(true);

    auto modelUpdated = [this](const QModelIndex& index = QModelIndex())
    {
        QModelIndex checkedIdx = index.isValid() ?
            index.sibling(index.row(), QnResourceListModel::CheckColumn) :
            m_controlsModel->index(0, QnResourceListModel::CheckColumn);

        bool checked = checkedIdx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        ui->resourcesTreeView->setEnabled(!checked);
        emit controlsChanged(checked);
        emit hasChangesChanged();
    };

    connect(m_controlsModel.data(), &QnResourceListModel::dataChanged, this, modelUpdated);
    connect(m_controlsModel.data(), &QnResourceListModel::modelReset, this, modelUpdated);
}

bool QnAccessibleResourcesWidget::resourcePassFilter(const QnResourcePtr& resource) const
{
    return resourcePassFilter(resource, context()->user(), m_filter);
}

bool QnAccessibleResourcesWidget::resourcePassFilter(const QnResourcePtr& resource, const QnUserResourcePtr& currentUser, QnResourceAccessFilter::Filter filter)
{
    switch (filter)
    {
        case QnResourceAccessFilter::MediaFilter:
            if (resource->hasFlags(Qn::desktop_camera))
                return false;
            return (resource->hasFlags(Qn::web_page) || resource->hasFlags(Qn::server_live_cam));

        case QnResourceAccessFilter::LayoutsFilter:
        {
            if (!currentUser)
                return false;

            if (!resource->hasFlags(Qn::layout))
                return false;

            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if (!layout)
                return false;

            /* Hide "Preview Search" layouts */
            if (layout->data().contains(Qn::LayoutSearchStateRole)) //TODO: #GDM make it consistent with QnWorkbenchLayout::isSearchLayout
                return false;

            if (qnResPool->isAutoGeneratedLayout(layout))
                return false;

            if (layout->isFile())
                return false;

            if (layout->hasFlags(Qn::local))
                return false;

            return layout->isShared() || layout->getParentId() == currentUser->getId();
        }

        default:
            break;
    }

    return false;
};

void QnAccessibleResourcesWidget::initResourcesModel()
{
    m_resourcesModel->setHasCheckboxes(true);
    m_resourcesModel->setUserCheckable(false);
    m_resourcesModel->setStatusIgnored(true);

    auto handleResourceAdded = [this](const QnResourcePtr& resource)
    {
        if (!resourcePassFilter(resource))
            return;

        if (m_resourcesModel->resources().contains(resource))
            return;

        m_resourcesModel->addResource(resource);
    };

    auto refreshModel = [this, handleResourceAdded]()
    {
        m_resourcesModel->setResources(QnResourceList());
        for (const QnResourcePtr& resource : qnResPool->getResources())
            handleResourceAdded(resource);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded, this, handleResourceAdded);

    if (m_filter == QnResourceAccessFilter::LayoutsFilter)
    {
        connect(qnResPool, &QnResourcePool::resourceAdded, this, [this, handleResourceAdded](const QnResourcePtr& resource)
        {
            if (!resource.dynamicCast<QnLayoutResource>())
                return;

            /* Looks like hack as we have no dynamic filter model and must maintain list manually.
             * Really the only scenario we should handle is when the layout becomes remote (after it is saved). */
            connect(resource.data(), &QnResource::flagsChanged, this, handleResourceAdded);
        });
    }
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this, refreshModel](const QnResourcePtr& resource)
    {
        disconnect(resource.data(), nullptr, this, nullptr);
        m_resourcesModel->removeResource(resource);
        if (resource == context()->user())
            refreshModel();
    });

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this, refreshModel);
    refreshModel();

    connect(m_resourcesModel.data(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    {
        if (roles.contains(Qt::CheckStateRole)
            && topLeft.column() <= QnResourceListModel::CheckColumn
            && bottomRight.column() >= QnResourceListModel::CheckColumn)
        {
            emit hasChangesChanged();
        }
    });
}

void QnAccessibleResourcesWidget::updateThumbnail(const QModelIndex& index)
{
    if (!index.isValid())
    {
        ui->namePlainText->hide();
        ui->previewWidget->hide();
        return;
    }

    QModelIndex baseIndex = index.column() == QnResourceListModel::NameColumn
        ? index
        : index.sibling(index.row(), QnResourceListModel::NameColumn);

    QString toolTip = baseIndex.data(Qt::ToolTipRole).toString();
    ui->namePlainText->setPlainText(toolTip);
    ui->namePlainText->show();

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        ui->previewWidget->setTargetResource(camera->getId());
        ui->previewWidget->show();
        ui->verticalLayout->activate();
    }
    else
    {
        ui->previewWidget->hide();
    }
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

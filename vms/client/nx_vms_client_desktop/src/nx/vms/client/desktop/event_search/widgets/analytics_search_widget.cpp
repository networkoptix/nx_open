// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_widget.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include <QtCore/QCollator>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QPalette>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollArea>

#include <api/server_rest_connection.h>
#include <client/desktop_client_message_processor.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/item_model_menu.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_context.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

static const core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

NX_DECLARE_COLORIZED_ICON(kDefaultAnalyticsIcon, "20x20/Outline/default.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kFrameIcon, "20x20/Outline/frame.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kPluginsIcon, "20x20/Outline/plugins.svg", kThemeSubstitutions)

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kNoObjectsTheme = {
    {QnIcon::Normal, {.primary = "dark16"}}};

NX_DECLARE_COLORIZED_ICON(
    kObjectsPlaceholderIcon, "64x64/Outline/noobjects.svg", kNoObjectsTheme)

using namespace std::chrono;
using namespace nx::analytics::taxonomy;
using namespace nx::vms::client::core::analytics;
using namespace core::analytics::taxonomy;

using nx::vms::client::core::AccessController;

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(AnalyticsSearchWidget::Private)
    AnalyticsSearchWidget* const q;

public:
    explicit Private(AnalyticsSearchWidget* q);

    void resetFilters();
    void updateAllowanceAndTaxonomy();
    std::vector<AbstractEngine*> engines() const;

    core::AnalyticsSearchSetup* analyticsSetup() const { return m_analyticsSetup.get(); }

public:
    QPointer<TaxonomyManager> taxonomyManager;
    nx::utils::ScopedConnection m_currentTaxonomyChangedConnection;
    nx::utils::ScopedConnection m_systemBecomeOfflineConnection;

private:
    void setupEngineSelection();
    void setupTypeSelection();
    void updateTaxonomy();
    void updateTaxonomyIfNeeded();
    void updateTypeButton();

    void setupAreaSelection();
    void updateAreaButtonAppearance();

    void setupDeviceSelection();

    bool ensureVisible(milliseconds timestamp, const nx::Uuid& trackId);
    void ensureVisibleAndFetchIfNeeded(milliseconds timestamp, const nx::Uuid& trackId);

    AbstractEngine* engineById(const nx::Uuid& id) const;

    QAction* addEngineMenuAction(QMenu* menu, const QString& title, const nx::Uuid& engineId);
    QAction* addObjectMenuAction(
        QMenu* menu,
        const QString& title,
        const QStringList& objectTypeIds);

private:
    AnalyticsSearchListModel* const m_model = qobject_cast<AnalyticsSearchListModel*>(q->model());
    std::unique_ptr<core::AnalyticsSearchSetup> m_analyticsSetup{
        new core::AnalyticsSearchSetup(m_model)};
    SelectableTextButton* const m_areaSelectionButton;
    SelectableTextButton* const m_engineSelectionButton;
    SelectableTextButton* const m_typeSelectionButton;
    AnalyticsFilterModel* const m_filterModel;
    DetectableObjectTypeModel* const m_objectTypeModel;
    QMenu* const m_engineMenu;
    ItemModelMenu* const m_objectTypeMenu;
    QCollator m_collator;

    std::optional<std::pair<milliseconds, nx::Uuid>> m_ensureVisibleRequest;
};

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget

AnalyticsSearchWidget::AnalyticsSearchWidget(WindowContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context->system()), parent),
    d(new Private(this))
{
    setRelevantControls(Control::defaults | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->icon(kObjectsPlaceholderIcon).pixmap(64, 64));

    addSearchAction(action(menu::OpenAdvancedSearchDialog));
    setSearchDelay(milliseconds(ini().analyticsSearchRequestDelayMs));

    connect(model(), &core::AbstractSearchListModel::isOnlineChanged, this,
        &AnalyticsSearchWidget::updateAllowance);

    d->updateAllowanceAndTaxonomy();

    HelpTopicAccessor::setHelpTopic(this, HelpTopic::Id::ObjectSearch);
}

AnalyticsSearchWidget::~AnalyticsSearchWidget()
{
}

void AnalyticsSearchWidget::resetFilters()
{
    base_type::resetFilters();
    d->resetFilters();
}

core::AnalyticsSearchSetup* AnalyticsSearchWidget::analyticsSetup() const
{
    return d->analyticsSetup();
}

QString AnalyticsSearchWidget::placeholderText(bool /*constrained*/) const
{
    return makePlaceholderText(tr("No objects"),
        tr("Try changing the filters or configure object detection in the camera plugin settings"));
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

bool AnalyticsSearchWidget::calculateAllowance() const
{
    if (!d->taxonomyManager)
        return false;

    const bool hasPermissions = model()->isOnline()
        && system()->accessController()->isDeviceAccessRelevant(
            nx::vms::api::AccessRight::viewArchive);

    return hasPermissions && !d->engines().empty();
}

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

AnalyticsSearchWidget::Private::Private(AnalyticsSearchWidget* q):
    q(q),
    taxonomyManager(q->system()->taxonomyManager()),
    m_areaSelectionButton(q->createCustomFilterButton()),
    m_engineSelectionButton(q->createCustomFilterButton()),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_filterModel(new AnalyticsFilterModel(taxonomyManager, this)),
    m_objectTypeModel(new DetectableObjectTypeModel(m_filterModel, this)),
    m_engineMenu(q->createDropdownMenu()),
    m_objectTypeMenu(new ItemModelMenu(q))
{
    NX_CRITICAL(m_model);

    m_objectTypeModel->setLiveTypesExcluded(true);
    m_objectTypeMenu->setModel(m_objectTypeModel);
    m_objectTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_objectTypeMenu->setWindowFlags(
        m_objectTypeMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    installEventHandler(q, QEvent::Show, this, &Private::updateTaxonomyIfNeeded);

    if (NX_ASSERT(taxonomyManager))
    {
        m_currentTaxonomyChangedConnection.reset(
            connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
                this, &AnalyticsSearchWidget::Private::updateAllowanceAndTaxonomy));
    }

    setupEngineSelection();
    setupTypeSelection();
    setupAreaSelection();
    setupDeviceSelection();

    connect(analyticsSetup(), &core::AnalyticsSearchSetup::attributeFiltersChanged,
        this, &Private::updateTypeButton);

    connect(m_filterModel, &AnalyticsFilterModel::objectTypesChanged,
        this, &Private::updateTypeButton);

    connect(analyticsSetup(), &core::AnalyticsSearchSetup::ensureVisibleRequested,
        this, &Private::ensureVisibleAndFetchIfNeeded);

    connect(m_model, &AnalyticsSearchListModel::pluginActionRequested,
        q->view(), &EventRibbon::pluginActionRequested);
    connect(m_filterModel, &AnalyticsFilterModel::enginesChanged,
        q, &AnalyticsSearchWidget::updateAllowance);

    connect(m_model, &core::AnalyticsSearchListModel::fetchFinished, this,
        [this](core::EventSearch::FetchResult result)
        {
            if (!m_ensureVisibleRequest)
                return;

            const auto request = *m_ensureVisibleRequest;
            m_ensureVisibleRequest = std::nullopt;

            if (result != core::EventSearch::FetchResult::cancelled
                && result != core::EventSearch::FetchResult::failed)
            {
                ensureVisible(request.first, request.second);
            }
        });

    auto setSystemContextToModels =
        [this, q](SystemContext* cameraContext)
        {
            if (!nx::vms::client::core::ini().allowCslObjectSearch)
                return;

            m_currentTaxonomyChangedConnection.reset();

            if (cameraContext)
            {
                if (taxonomyManager = cameraContext->taxonomyManager())
                {
                    if (!cameraContext->messageProcessor())
                    {
                        // It is cross-system context case.
                        cameraContext->createMessageProcessor<QnDesktopClientMessageProcessor>(
                            taxonomyManager);
                    }

                    m_currentTaxonomyChangedConnection.reset(
                        connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
                            this, &AnalyticsSearchWidget::Private::updateAllowanceAndTaxonomy));
                }
            }
            else
            {
                taxonomyManager = nullptr;
            }

            m_filterModel->setTaxonomyManager(taxonomyManager);

            m_model->setSystemContext(cameraContext);
            q->commonSetup()->setSystemContext(cameraContext);

            updateAllowanceAndTaxonomy();
        };

     connect(
        appContext()->cloudCrossSystemManager(), &core::CloudCrossSystemManager::systemAboutToBeLost,
        this,
        [this, q, setSystemContextToModels](const QString& systemId)
        {
            core::CloudCrossSystemContext* crossSystemContext =
                appContext()->cloudCrossSystemManager()->systemContext(systemId);

            if (crossSystemContext->systemContext() != q->commonSetup()->systemContext())
                return;

            // We should clean any links to the lost system context from the models before it is
            // destroyed.
            setSystemContextToModels(nullptr);
        });

    const auto handleCurrentResourceChanged =
        [this, setSystemContextToModels]()
        {
            m_systemBecomeOfflineConnection.reset();

            SystemContext* cameraContext = nullptr;

            const auto currentLayout =
                this->q->windowContext()->workbench()->currentLayout()->resource();
            const bool cslMode = currentLayout->flags().testFlag(Qn::cross_system);

            constexpr Controls kControls = Control::timeSelector | Control::freeTextFilter
                | Control::previewsToggler | Control::footersToggler;

            this->q->setRelevantControls(cslMode
                ? kControls | Control::cameraSelectionDisplay
                : kControls | Control::cameraSelector);

            if (const auto currentResource =
                this->q->windowContext()->navigator()->currentResource())
            {
                cameraContext = SystemContext::fromResource(currentResource);
            }
            else if (!cslMode)
            {
                cameraContext = appContext()->currentSystemContext();
            }

            setSystemContextToModels(cameraContext);

            if (!cameraContext)
                return;

            const auto systemId = helpers::getTargetSystemId(cameraContext->moduleInformation());
            auto systemDescription = appContext()->systemFinder()->getSystem(systemId);
            if (!systemDescription)
                return;

            m_systemBecomeOfflineConnection.reset(
                connect(
                    systemDescription.get(),
                    &nx::vms::client::core::SystemDescription::onlineStateChanged,
                    this,
                    [this, systemDescription, setSystemContextToModels]()
                    {
                        if (systemDescription->isOnline())
                        {
                            const auto cameraContext = SystemContext::fromResource(
                                this->q->windowContext()->navigator()->currentResource());
                            setSystemContextToModels(cameraContext);
                        }
                        else
                        {
                            setSystemContextToModels(nullptr);
                        }
                    }));
        };

    connect(q->windowContext()->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, handleCurrentResourceChanged);

    // For the case when an empty CSL was changed to an empty local layout,
    // and the current system has objects.
    connect(q->windowContext()->workbench(), &Workbench::currentLayoutChanged, this,
        [this, handleCurrentResourceChanged]()
        {
            const auto currentResource = this->q->windowContext()->navigator()->currentResource();
            if (!currentResource)
                handleCurrentResourceChanged();
        });
}

void AnalyticsSearchWidget::Private::resetFilters()
{
    m_typeSelectionButton->deactivate();
    m_areaSelectionButton->deactivate();
}

void AnalyticsSearchWidget::Private::setupEngineSelection()
{
    m_engineSelectionButton->setDeactivatedText(tr("Any plugin"));
    m_engineSelectionButton->setIcon(qnSkin->icon(kPluginsIcon));

    m_engineSelectionButton->setMenu(m_engineMenu);

    connect(m_engineSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_analyticsSetup->setEngine({});
        });

    connect(m_analyticsSetup.get(), &core::AnalyticsSearchSetup::engineChanged, this,
        [this]()
        {
            const auto engine = engineById(m_analyticsSetup->engine());
            if (engine)
                m_engineSelectionButton->setText(engine->name());

            m_engineSelectionButton->setState(engine
                ? SelectableTextButton::State::unselected
                : SelectableTextButton::State::deactivated);

            updateTypeButton();
        });
}

void AnalyticsSearchWidget::Private::setupTypeSelection()
{
    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(
        qnSkin->icon(kDefaultAnalyticsIcon));

    m_typeSelectionButton->setMenu(m_objectTypeMenu);

    connect(m_objectTypeMenu, &ItemModelMenu::itemTriggered, this,
        [this](const QModelIndex& index)
        {
            QStringList typeIds = index.isValid()
                ? index.data(DetectableObjectTypeModel::IdsRole).toStringList()
                : QStringList();

            m_analyticsSetup->setObjectTypes(typeIds);
        });

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_analyticsSetup->setObjectTypes({});
        });

    connect(m_analyticsSetup.get(), &core::AnalyticsSearchSetup::objectTypesChanged,
        this, &Private::updateTypeButton);
}

void AnalyticsSearchWidget::Private::updateTypeButton()
{
    const auto objectTypeIds = m_analyticsSetup->objectTypes();
    ObjectType* objectType = m_objectTypeModel->sourceModel()->findFilterObjectType(objectTypeIds);

    const auto engine = engineById(m_analyticsSetup->engine());
    m_objectTypeModel->setEngine(engine);
    m_typeSelectionButton->setVisible(!m_filterModel->objectTypes().empty());

    if (objectType)
    {
        const int numFilters = analyticsSetup()->attributeFilters().size();
        m_typeSelectionButton->setText(numFilters > 0
            ? tr("%1 with %n attributes", "", numFilters).arg(objectType->name())
            : objectType->name());

        m_typeSelectionButton->setIcon(core::Skin::colorize(qnSkin->pixmap(
            core::analytics::IconManager::instance()->iconPath(objectType->icon())),
            m_typeSelectionButton->palette().color(QPalette::Inactive, QPalette::WindowText)));

        m_typeSelectionButton->setState(SelectableTextButton::State::unselected);
    }
    else
    {
        m_typeSelectionButton->setIcon(
            qnSkin->icon(kDefaultAnalyticsIcon));
        m_typeSelectionButton->deactivate();
    }
}

void AnalyticsSearchWidget::Private::updateTaxonomyIfNeeded()
{
    if (q->isAllowed() && q->isVisible())
        updateTaxonomy();
}

void AnalyticsSearchWidget::Private::updateTaxonomy()
{
    if (!NX_ASSERT(taxonomyManager && taxonomyManager->currentTaxonomy()))
        return;

    auto relevantEngines = m_filterModel->engines();
    const bool noEngineSelection = relevantEngines.size() < 2;

    m_engineSelectionButton->setHidden(noEngineSelection);
    WidgetUtils::clearMenu(m_engineMenu);

    if (noEngineSelection)
    {
        m_engineSelectionButton->deactivate();
        updateTypeButton();
        return;
    }

    const auto currentSelection = m_model->selectedEngine();
    bool currentSelectionStillAvailable = false;

    addEngineMenuAction(m_engineMenu, m_engineSelectionButton->deactivatedText(), {});
    m_engineMenu->addSeparator();

    std::sort(relevantEngines.begin(), relevantEngines.end(),
        [this](AbstractEngine* left, AbstractEngine* right)
        {
            return m_collator.compare(left->name(), right->name()) < 0;
        });

    for (const auto& engine: relevantEngines)
    {
        const nx::Uuid id(engine->id());
        addEngineMenuAction(m_engineMenu, engine->name(), id);

        if (!currentSelectionStillAvailable && currentSelection == id)
            currentSelectionStillAvailable = true;
    }

    if (!currentSelectionStillAvailable)
        m_engineSelectionButton->deactivate();

    updateTypeButton();
}

void AnalyticsSearchWidget::Private::updateAllowanceAndTaxonomy()
{
    q->updateAllowance();
    updateTaxonomyIfNeeded();
}

void AnalyticsSearchWidget::Private::setupAreaSelection()
{
    m_areaSelectionButton->setAccented(true);
    m_areaSelectionButton->setDeactivatedText(tr("Select area"));
    m_areaSelectionButton->setIcon(qnSkin->icon(kFrameIcon));

    connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
            {
                m_analyticsSetup->setAreaSelectionActive(false);
                m_analyticsSetup->setArea({});
            }
        });

    connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
        [this]()
        {
            q->commonSetup()->setCameraSelection(core::EventSearch::CameraSelection::current);
            m_analyticsSetup->setAreaSelectionActive(true);
        });

    connect(m_analyticsSetup.get(), &core::AnalyticsSearchSetup::areaEnabledChanged,
        this, &Private::updateAreaButtonAppearance);

    connect(m_analyticsSetup.get(), &core::AnalyticsSearchSetup::areaChanged,
        this, &Private::updateAreaButtonAppearance);

    connect(m_analyticsSetup.get(), &core::AnalyticsSearchSetup::areaSelectionActiveChanged,
        this, &Private::updateAreaButtonAppearance);
}

void AnalyticsSearchWidget::Private::updateAreaButtonAppearance()
{
    m_areaSelectionButton->setState(
        m_analyticsSetup->isAreaSelected() || m_analyticsSetup->areaSelectionActive()
            ? SelectableTextButton::State::unselected
            : SelectableTextButton::State::deactivated);

    m_areaSelectionButton->setAccented(m_analyticsSetup->areaSelectionActive());
    m_areaSelectionButton->setText(m_areaSelectionButton->accented()
        ? tr("Select some area on the video...")
        : tr("In selected area"));
}

void AnalyticsSearchWidget::Private::setupDeviceSelection()
{
    connect(q->commonSetup(), &CommonObjectSearchSetup::selectedCamerasChanged,
        [this]
        {
            m_filterModel->setSelectedDevices(q->commonSetup()->selectedCameras());
            updateTypeButton();
        });
}

std::vector<AbstractEngine*> AnalyticsSearchWidget::Private::engines() const
{
    return m_filterModel->engines();
}

AbstractEngine* AnalyticsSearchWidget::Private::engineById(const nx::Uuid& id) const
{
    if (id.isNull())
        return nullptr;

    const auto relevantEngines = m_filterModel->engines();

    const auto it = std::find_if(relevantEngines.cbegin(), relevantEngines.cend(),
        [id](AbstractEngine* engine) { return nx::Uuid(engine->id()) == id; });

    return it != relevantEngines.cend() ? *it : nullptr;
}

QAction* AnalyticsSearchWidget::Private::addEngineMenuAction(
    QMenu* menu, const QString& title, const nx::Uuid& engineId)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, engineId]() { m_analyticsSetup->setEngine(engineId); });

    return action;
}

QAction* AnalyticsSearchWidget::Private::addObjectMenuAction(
    QMenu* menu, const QString& title, const QStringList& objectTypeIds)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, objectTypeIds]() { m_analyticsSetup->setObjectTypes(objectTypeIds); });

    return action;
}

void AnalyticsSearchWidget::Private::ensureVisibleAndFetchIfNeeded(
    milliseconds timestamp, const nx::Uuid& trackId)
{
    if (ensureVisible(timestamp, trackId))
        return;

    m_model->cancelFetch();
    m_ensureVisibleRequest = std::make_pair(timestamp, trackId);
    m_model->fetchData({core::EventSearch::FetchDirection::newer, timestamp});
}

bool AnalyticsSearchWidget::Private::ensureVisible(milliseconds timestamp, const nx::Uuid& trackId)
{
    const auto range = m_model->find(timestamp);
    for (int row = range.first; row < range.second; ++row)
    {
        const auto currentIndex = m_model->index(row);
        if (currentIndex.data(core::ObjectTrackIdRole).value<nx::Uuid>() == trackId)
        {
            q->view()->ensureVisible(row);
            return true;
        }
    }

    return false;
}

} // namespace nx::vms::client::desktop

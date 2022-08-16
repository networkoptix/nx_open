// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_widget.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include <QtCore/QCollator>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtGui/QPalette>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QAction>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollArea>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/analytics/analytics_icon_manager.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::analytics::taxonomy;
using namespace nx::vms::client::desktop::analytics;
using namespace nx::vms::client::desktop::ui;

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(AnalyticsSearchWidget::Private)
    AnalyticsSearchWidget* const q;

public:
    explicit Private(AnalyticsSearchWidget* q);

    void resetFilters();
    void updateTaxonomyIfNeeded();

    AnalyticsSearchSetup* analyticsSetup() const { return m_analyticsSetup.get(); }

public:
    const QPointer<TaxonomyManager> taxonomyManager;

private:
    void setupEngineSelection();
    void setupTypeSelection();
    void updateTaxonomy();
    void updateObjectTypes();
    void updateTypeButtonState();

    void setupAreaSelection();
    void updateAreaButtonAppearance();

    bool ensureVisible(milliseconds timestamp, const QnUuid& trackId);
    void ensureVisibleAndFetchIfNeeded(
        milliseconds timestamp, const QnUuid& trackId, const QnTimePeriod& proposedTimeWindow);

    AbstractEngine* engineById(const QnUuid& id) const;

    QAction* addEngineMenuAction(QMenu* menu, const QString& title, const QnUuid& engineId);
    QAction* addObjectMenuAction(QMenu* menu, const QString& title, const QString& objectTypeId);

private:
    AnalyticsSearchListModel* const m_model = qobject_cast<AnalyticsSearchListModel*>(q->model());
    std::unique_ptr<AnalyticsSearchSetup> m_analyticsSetup{new AnalyticsSearchSetup(m_model)};
    SelectableTextButton* const m_areaSelectionButton;
    SelectableTextButton* const m_engineSelectionButton;
    SelectableTextButton* const m_typeSelectionButton;
    QMenu* const m_engineMenu;
    QMenu* const m_objectTypeMenu;
    bool m_areaSelectionEnabled = false;
    QCollator m_collator;

    std::optional<std::pair<milliseconds, QnUuid>> m_ensureVisibleRequest;
};

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::defaults | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("left_panel/placeholders/objects.svg"));
    commonSetup()->setCameraSelection(RightPanel::CameraSelection::layout);

    addSearchAction(action(ui::action::OpenAdvancedSearchDialog));

    connect(model(), &AbstractSearchListModel::isOnlineChanged, this,
        &AnalyticsSearchWidget::updateAllowance);
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        &AnalyticsSearchWidget::updateAllowance);

    connect(action(action::ObjectSearchModeAction), &QAction::toggled, this,
        [this](bool on)
        {
            if (!on && commonSetup()->cameraSelection() == RightPanel::CameraSelection::current)
                resetFilters();
        });

    updateAllowance();
    d->updateTaxonomyIfNeeded();
}

AnalyticsSearchWidget::~AnalyticsSearchWidget()
{
}

void AnalyticsSearchWidget::resetFilters()
{
    base_type::resetFilters();
    commonSetup()->setCameraSelection(RightPanel::CameraSelection::layout);
    d->resetFilters();
}

AnalyticsSearchSetup* AnalyticsSearchWidget::analyticsSetup() const
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
        && accessController()->hasGlobalPermission(GlobalPermission::viewArchive);

    return hasPermissions && !d->taxonomyManager->relevantEngines().empty();
}

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

AnalyticsSearchWidget::Private::Private(AnalyticsSearchWidget* q):
    q(q),
    taxonomyManager(qnClientCoreModule->mainQmlEngine()->singletonInstance<TaxonomyManager*>(
        qmlTypeId("nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager"))),
    m_areaSelectionButton(q->createCustomFilterButton()),
    m_engineSelectionButton(q->createCustomFilterButton()),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_engineMenu(q->createDropdownMenu()),
    m_objectTypeMenu(q->createDropdownMenu())
{
    NX_CRITICAL(m_model);

    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    if (NX_ASSERT(taxonomyManager))
    {
        installEventHandler(q, QEvent::Show, this, &Private::updateTaxonomyIfNeeded);

        connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged, this,
            [this]()
            {
                this->q->updateAllowance();
                updateTaxonomyIfNeeded();
            });
    }

    setupEngineSelection();
    setupTypeSelection();
    setupAreaSelection();

    connect(analyticsSetup(), &AnalyticsSearchSetup::attributeFiltersChanged,
        this, &Private::updateTypeButtonState);

    connect(analyticsSetup(), &AnalyticsSearchSetup::ensureVisibleRequested,
        this, &Private::ensureVisibleAndFetchIfNeeded);

    connect(m_model, &AnalyticsSearchListModel::pluginActionRequested,
        q->view(), &EventRibbon::pluginActionRequested);

    connect(m_model, &AnalyticsSearchListModel::fetchFinished, this,
        [this](RightPanel::FetchResult result)
        {
            if (!m_ensureVisibleRequest)
                return;

            const auto request = *m_ensureVisibleRequest;
            m_ensureVisibleRequest = std::nullopt;

            if (result != RightPanel::FetchResult::cancelled
                && result != RightPanel::FetchResult::failed)
            {
                ensureVisible(request.first, request.second);
            }
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
    m_engineSelectionButton->setIcon(qnSkin->icon("text_buttons/plugins.svg"));

    m_engineSelectionButton->setMenu(m_engineMenu);

    connect(m_engineSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_analyticsSetup->setEngine({});
        });

    connect(m_analyticsSetup.get(), &AnalyticsSearchSetup::engineChanged, this,
        [this]()
        {
            const auto engine = engineById(m_analyticsSetup->engine());
            if (engine)
                m_engineSelectionButton->setText(engine->name());

            m_engineSelectionButton->setState(engine
                ? SelectableTextButton::State::unselected
                : SelectableTextButton::State::deactivated);

            updateObjectTypes();
        });
}

void AnalyticsSearchWidget::Private::setupTypeSelection()
{
    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));

    m_typeSelectionButton->setMenu(m_objectTypeMenu);

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_analyticsSetup->setObjectType({});
        });

    connect(m_analyticsSetup.get(), &AnalyticsSearchSetup::objectTypeChanged,
        this, &Private::updateTypeButtonState);
}

void AnalyticsSearchWidget::Private::updateTypeButtonState()
{
    const auto taxonomy = taxonomyManager ? taxonomyManager->currentTaxonomy() : nullptr;
    const auto objectType = taxonomy
        ? taxonomy->objectTypeById(m_analyticsSetup->objectType())
        : nullptr;

    if (objectType)
    {
        const int numFilters = analyticsSetup()->attributeFilters().size();
        m_typeSelectionButton->setText(numFilters > 0
            ? tr("%1 with %n attributes", "", numFilters).arg(objectType->name())
            : objectType->name());

        m_typeSelectionButton->setIcon(Skin::colorize(qnSkin->pixmap(
            IconManager::instance()->iconPath(objectType->icon())),
            m_typeSelectionButton->palette().color(QPalette::Inactive, QPalette::WindowText)));
    }
    else
    {
        m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));
    }

    m_typeSelectionButton->setState(objectType
        ? SelectableTextButton::State::unselected
        : SelectableTextButton::State::deactivated);
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

    auto relevantEngines = taxonomyManager->relevantEngines().toList();
    const bool noEngineSelection = relevantEngines.size() < 2;

    m_engineSelectionButton->setHidden(noEngineSelection);
    WidgetUtils::clearMenu(m_engineMenu);

    if (noEngineSelection)
    {
        m_engineSelectionButton->deactivate();
        updateObjectTypes();
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
        const QnUuid id(engine->id());
        addEngineMenuAction(m_engineMenu, engine->name(), id);

        if (!currentSelectionStillAvailable && currentSelection == id)
            currentSelectionStillAvailable = true;
    }

    if (!currentSelectionStillAvailable)
        m_engineSelectionButton->deactivate();

    updateObjectTypes();
}

void AnalyticsSearchWidget::Private::updateObjectTypes()
{
    if (!NX_ASSERT(taxonomyManager && taxonomyManager->currentTaxonomy()))
        return;

    const QString currentSelection = m_model->selectedObjectType();
    bool currentSelectionStillAvailable = false;

    const auto engine = engineById(m_analyticsSetup->engine());

    QHash<AbstractObjectType*, QVector<AbstractObjectType*>> relevantDerivedTypes;

    const auto processTypesRecursively = nx::utils::y_combinator(
        [this, engine, &relevantDerivedTypes](
            auto processTypesRecursively, AbstractObjectType* baseType, auto derivedTypes) -> void
        {
            QVector<AbstractObjectType*> relevantTypes;
            for (const auto type: derivedTypes)
            {
                if (type->isReachable() && !type->isLiveOnly() && !type->isNonIndexable()
                    && taxonomyManager->isRelevantForEngine(type, engine))
                {
                    relevantTypes.push_back(type);
                }
            }

            std::sort(relevantTypes.begin(), relevantTypes.end(),
                [this](AbstractObjectType* left, AbstractObjectType* right)
                {
                    return m_collator.compare(left->name(), right->name()) < 0;
                });

            relevantDerivedTypes[baseType] = relevantTypes;

            for (const auto type: relevantTypes)
                processTypesRecursively(type, type->derivedTypes());
        });

    const auto addItemsRecursively = nx::utils::y_combinator(
        [this, &relevantDerivedTypes, currentSelection, &currentSelectionStillAvailable](
            auto addItemsRecursively, QMenu* menu, auto objectTypes) -> void
        {
            for (const auto& type: objectTypes)
            {
                if (!currentSelectionStillAvailable && currentSelection == type->id())
                    currentSelectionStillAvailable = true;

                const auto& derivedTypes = relevantDerivedTypes[type];
                if (derivedTypes.empty())
                {
                    addObjectMenuAction(menu, type->name(), type->id());
                }
                else
                {
                    const auto subMenu = menu->addMenu(type->name());
                    q->fixMenuFlags(subMenu);
                    addObjectMenuAction(subMenu, tr("Any subtype"), type->id());
                    subMenu->addSeparator();
                    addItemsRecursively(subMenu, derivedTypes);
                }
            }
        });

    WidgetUtils::clearMenu(m_objectTypeMenu);
    addObjectMenuAction(m_objectTypeMenu, m_typeSelectionButton->deactivatedText(), {});
    m_objectTypeMenu->addSeparator();

    processTypesRecursively(nullptr, taxonomyManager->currentTaxonomy()->rootObjectTypes());
    addItemsRecursively(m_objectTypeMenu, relevantDerivedTypes[nullptr]);

    if (!currentSelectionStillAvailable)
        m_typeSelectionButton->deactivate();
}

void AnalyticsSearchWidget::Private::setupAreaSelection()
{
    m_areaSelectionButton->setAccented(true);
    m_areaSelectionButton->setDeactivatedText(tr("Select area"));
    m_areaSelectionButton->setIcon(qnSkin->icon("text_buttons/area.png"));

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
            q->commonSetup()->setCameraSelection(RightPanel::CameraSelection::current);
            m_analyticsSetup->setAreaSelectionActive(true);
        });

    connect(m_analyticsSetup.get(), &AnalyticsSearchSetup::areaEnabledChanged,
        this, &Private::updateAreaButtonAppearance);

    connect(m_analyticsSetup.get(), &AnalyticsSearchSetup::areaChanged,
        this, &Private::updateAreaButtonAppearance);

    connect(m_analyticsSetup.get(), &AnalyticsSearchSetup::areaSelectionActiveChanged,
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

AbstractEngine* AnalyticsSearchWidget::Private::engineById(const QnUuid& id) const
{
    if (id.isNull())
        return nullptr;

    const auto relevantEngines = taxonomyManager
        ? taxonomyManager->relevantEngines()
        : decltype(taxonomyManager->relevantEngines()){};

    const auto it = std::find_if(relevantEngines.cbegin(), relevantEngines.cend(),
        [id](AbstractEngine* engine) { return QnUuid(engine->id()) == id; });

    return it != relevantEngines.cend() ? *it : nullptr;
}

QAction* AnalyticsSearchWidget::Private::addEngineMenuAction(
    QMenu* menu, const QString& title, const QnUuid& engineId)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, engineId]() { m_analyticsSetup->setEngine(engineId); });

    return action;
}

QAction* AnalyticsSearchWidget::Private::addObjectMenuAction(
    QMenu* menu, const QString& title, const QString& objectTypeId)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, objectTypeId]() { m_analyticsSetup->setObjectType(objectTypeId); });

    return action;
}

void AnalyticsSearchWidget::Private::ensureVisibleAndFetchIfNeeded(
    milliseconds timestamp, const QnUuid& trackId, const QnTimePeriod& proposedTimeWindow)
{
    if (ensureVisible(timestamp, trackId))
        return;

    if (!NX_ASSERT(proposedTimeWindow.contains(timestamp)))
        return;

    m_model->cancelFetch();
    m_ensureVisibleRequest = std::make_pair(timestamp, trackId);
    m_model->fetchWindow(proposedTimeWindow);
}

bool AnalyticsSearchWidget::Private::ensureVisible(milliseconds timestamp, const QnUuid& trackId)
{
    const auto range = m_model->find(timestamp);
    for (int row = range.first; row < range.second; ++row)
    {
        const auto currentIndex = m_model->index(row);
        if (currentIndex.data(Qn::ObjectTrackIdRole).value<QnUuid>() == trackId)
        {
            q->view()->ensureVisible(row);
            return true;
        }
    }

    return false;
}

} // namespace nx::vms::client::desktop

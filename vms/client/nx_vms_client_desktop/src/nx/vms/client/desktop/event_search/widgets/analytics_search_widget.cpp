#include "analytics_search_widget.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
#include <ui/style/skin.h>

#include <nx/analytics/descriptor_list_manager.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    AnalyticsSearchWidget* const q;

public:
    explicit Private(AnalyticsSearchWidget* q);

    QRectF filterRect() const;
    void setFilterRect(const QRectF& value);

    bool areaSelectionEnabled() const;

    void resetFilters();

private:
    void setupTypeSelection();
    void updateTypeMenu();

    void setupAreaSelection();
    void setAreaSelectionEnabled(bool value);
    void updateAreaButtonAppearance();

    void updateAvailableObjectTypes();

    QAction* addMenuAction(QMenu* menu, const QString& title, const QString& objectType);

private:
    AnalyticsSearchListModel* const m_model;
    SelectableTextButton* const m_typeSelectionButton;
    SelectableTextButton* const m_areaSelectionButton;
    QMenu* const m_objectTypeMenu;
    QPointer<QAction> m_defaultAction;
    bool m_areaSelectionEnabled = false;

    using ObjectTypeDescriptors = std::map<QString, nx::vms::api::analytics::ObjectTypeDescriptor>;
    struct PluginInfo
    {
        QString name;
        ObjectTypeDescriptors objectTypes;

        bool operator<(const PluginInfo& other) const
        {
            QCollator collator;
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            return collator.compare(name, other.name) < 0;
        }
    };
};

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::defaults | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/analytics.png"));
    selectCameras(AbstractSearchWidget::Cameras::layout);
}

AnalyticsSearchWidget::~AnalyticsSearchWidget()
{
}

void AnalyticsSearchWidget::resetFilters()
{
    base_type::resetFilters();
    selectCameras(AbstractSearchWidget::Cameras::layout);
    d->resetFilters();
}

void AnalyticsSearchWidget::setFilterRect(const QRectF& value)
{
    d->setFilterRect(value.intersected({0, 0, 1, 1}));
}

QRectF AnalyticsSearchWidget::filterRect() const
{
    return d->filterRect();
}

bool AnalyticsSearchWidget::areaSelectionEnabled() const
{
    return d->areaSelectionEnabled();
}

QString AnalyticsSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No objects") : tr("No objects detected");
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

bool AnalyticsSearchWidget::calculateAllowance() const
{
    return !commonModule()->analyticsDescriptorListManager()
        ->allDescriptorsInTheSystem<nx::vms::api::analytics::ObjectTypeDescriptor>().empty();
}

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

AnalyticsSearchWidget::Private::Private(AnalyticsSearchWidget* q):
    q(q),
    m_model(qobject_cast<AnalyticsSearchListModel*>(q->model())),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_areaSelectionButton(q->createCustomFilterButton()),
    m_objectTypeMenu(q->createDropdownMenu())
{
    NX_CRITICAL(m_model);

    setupTypeSelection();
    setupAreaSelection();

    connect(q, &AbstractSearchWidget::textFilterChanged,
        m_model, &AnalyticsSearchListModel::setFilterText);

    // Update analytics events submenu when servers are added to or removed from the system.
    const auto handleServerAddedOrRemoved =
        [this](const QnResourcePtr& resource)
        {
            if (!m_model->isOnline())
                return;

            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
                updateAvailableObjectTypes();
        };

    auto serverChangesListener = new QnResourceChangesListener(q);
    serverChangesListener->connectToResources<QnMediaServerResource>(
        &QnMediaServerResource::propertyChanged,
        this, &Private::updateAvailableObjectTypes);

    connect(q->resourcePool(), &QnResourcePool::resourceAdded, this, handleServerAddedOrRemoved);
    connect(q->resourcePool(), &QnResourcePool::resourceRemoved, this, handleServerAddedOrRemoved);

    connect(m_model, &AbstractSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            if (isOnline)
                updateAvailableObjectTypes();
        });

    if (m_model->isOnline())
        updateAvailableObjectTypes();
}

QRectF AnalyticsSearchWidget::Private::filterRect() const
{
    return m_model->filterRect();
}

void AnalyticsSearchWidget::Private::setFilterRect(const QRectF& value)
{
    if (qFuzzyEquals(m_model->filterRect(), value))
        return;

    m_model->setFilterRect(value);
    updateAreaButtonAppearance();

    emit q->filterRectChanged(m_model->filterRect());
}

bool AnalyticsSearchWidget::Private::areaSelectionEnabled() const
{
    return m_areaSelectionEnabled;
}

void AnalyticsSearchWidget::Private::resetFilters()
{
    m_typeSelectionButton->deactivate();
}

void AnalyticsSearchWidget::Private::updateAvailableObjectTypes()
{
    q->updateAllowance();
    updateTypeMenu();
}

void AnalyticsSearchWidget::Private::setupTypeSelection()
{
    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));

    m_typeSelectionButton->setMenu(m_objectTypeMenu);

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated && m_defaultAction)
                m_defaultAction->trigger();
        });
}

void AnalyticsSearchWidget::Private::updateTypeMenu()
{
    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));

    using namespace nx::vms::event;

    const QString currentSelection = m_model->selectedObjectType();
    bool currentSelectionStillAvailable = false;

    const auto descriptorListManager = q->commonModule()->analyticsDescriptorListManager();
    const auto allObjectTypes = descriptorListManager
        ->allDescriptorsInTheSystem<nx::vms::api::analytics::ObjectTypeDescriptor>();

    const auto allAnalyticsPlugins = descriptorListManager
        ->allDescriptorsInTheSystem<nx::vms::api::analytics::PluginDescriptor>();

    QHash<QString, PluginInfo> pluginsById;
    for (const auto& [pluginId, descriptor]: allAnalyticsPlugins)
        pluginsById[pluginId].name = descriptor.name;

    for (const auto& entry: allObjectTypes)
    {
        for (const auto& path: entry.second.paths)
            pluginsById[path.pluginId].objectTypes.insert(entry);
    }

    QVector<PluginInfo> plugins;
    for (const auto& pluginInfo: pluginsById)
    {
        if (!pluginInfo.objectTypes.empty())
            plugins.push_back(pluginInfo);
    }

    std::sort(plugins.begin(), plugins.end());
    const bool severalPlugins = plugins.size() > 1;

    QMenu* currentMenu = m_objectTypeMenu;
    m_objectTypeMenu->clear();

    for (const auto& plugin: plugins)
    {
        if (severalPlugins)
        {
            const auto pluginName = plugin.name.isEmpty()
                ? QString("<%1>").arg(tr("unnamed analytics plugin"))
                : plugin.name;

            currentMenu = m_objectTypeMenu->addMenu(pluginName);

            currentMenu->setWindowFlags(
                currentMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);
        }

        for (const auto entry: plugin.objectTypes)
        {
            const auto& descriptor = entry.second;
            addMenuAction(currentMenu, descriptor.item.name, descriptor.getId());

            if (!currentSelectionStillAvailable && currentSelection == descriptor.getId())
                currentSelectionStillAvailable = true;
        }
    }

    m_objectTypeMenu->addSeparator();
    m_defaultAction = addMenuAction(m_objectTypeMenu, tr("Any type"), {});

    if (!currentSelectionStillAvailable)
        m_model->setSelectedObjectType({});
}

void AnalyticsSearchWidget::Private::setupAreaSelection()
{
    m_areaSelectionButton->setAccented(true);
    m_areaSelectionButton->setDeactivatedText(tr("Select area"));
    m_areaSelectionButton->setIcon(qnSkin->icon("text_buttons/area.png"));
    m_areaSelectionButton->hide();

    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
        [this]() { setAreaSelectionEnabled(q->selectedCameras() == Cameras::current); });

    connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                setFilterRect({});
        });

    connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
        [this]()
        {
            m_areaSelectionButton->setText(tr("Select some area on the video..."));
            m_areaSelectionButton->setAccented(true);
            m_areaSelectionButton->setState(SelectableTextButton::State::unselected);
            emit q->areaSelectionRequested({});
        });
}

void AnalyticsSearchWidget::Private::setAreaSelectionEnabled(bool value)
{
    if (m_areaSelectionEnabled == value)
        return;

    m_areaSelectionEnabled = value;
    m_areaSelectionButton->setVisible(m_areaSelectionEnabled);
    emit q->areaSelectionEnabledChanged(m_areaSelectionEnabled, {});
}

void AnalyticsSearchWidget::Private::updateAreaButtonAppearance()
{
    if (!m_areaSelectionEnabled)
        return;

    m_areaSelectionButton->setState(m_model->filterRect().isValid()
        ? SelectableTextButton::State::unselected
        : SelectableTextButton::State::deactivated);

    m_areaSelectionButton->setAccented(false);
    m_areaSelectionButton->setText(tr("In selected area"));
}

QAction* AnalyticsSearchWidget::Private::addMenuAction(
    QMenu* menu, const QString& title, const QString& objectType)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, action, objectType]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(objectType.isEmpty()
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            m_model->setSelectedObjectType(objectType);
        });

    return action;
}

} // namespace nx::vms::client::desktop

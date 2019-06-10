#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_properties.h>

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

#include <health/system_health_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <ui/workbench/workbench_context.h>

#include <utils/resource_property_adaptors.h>

using namespace nx;
using namespace nx::vms::client::desktop;

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupSettingsWidget),
    m_businessRulesCheckBoxes(),
    m_systemHealthCheckBoxes(),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this)),
    m_updating(false),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    ui->setupUi(this);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (vms::api::EventType eventType : vms::event::allEvents())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_helper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    static_assert(QnSystemHealth::Count < 64, "We are packing messages to single quint64");

    for (auto messageType: QnSystemHealth::allVisibleMessageTypes())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
        &QnPopupSettingsWidget::loadDataToUi);

    connect(ui->showAllCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            // TODO: #GDM #Common also update checked state!
            // TODO: #GDM #Common maybe tristate for 'show all' checkbox would be better.
            for (QCheckBox* checkbox : m_businessRulesCheckBoxes)
                checkbox->setEnabled(!checked);

            for (QCheckBox* checkbox : m_systemHealthCheckBoxes)
                checkbox->setEnabled(!checked);

            emit hasChangesChanged();
        });

}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::loadDataToUi()
{
    if (m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    bool all = true;

    QSet<QnSystemHealth::MessageType> messageTypes = qnSettings->popupSystemHealth();

    for (auto messageType: QnSystemHealth::allVisibleMessageTypes())
    {
        const bool checked = messageTypes.contains(messageType);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);

        all &= checked;
    }

    if (context()->user())
        m_adaptor->setResource(context()->user());

    QList<vms::api::EventType> watchedEvents = context()->user()
        ? m_adaptor->watchedEvents()
        : vms::event::allEvents();

    for (vms::api::EventType eventType : m_businessRulesCheckBoxes.keys())
    {
        bool checked = watchedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);

    ui->businessEventsGroupBox->setEnabled(context()->user());
}

void QnPopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, "Should never get here while updating");
    QScopedValueRollback<bool> guard(m_updating, true);

    if (context()->user())
    {
        m_adaptor->setWatchedEvents(watchedEvents());
        m_adaptor->saveToResource();
        resourcePropertyDictionary()->saveParamsAsync(context()->user()->getId());
    }

    qnSettings->setPopupSystemHealth(watchedSystemHealth());
}

bool QnPopupSettingsWidget::hasChanges() const
{
    return qnSettings->popupSystemHealth() != watchedSystemHealth()
        || (context()->user() && m_adaptor->watchedEvents() != watchedEvents());
}

QList<vms::api::EventType> QnPopupSettingsWidget::watchedEvents() const
{
    if (ui->showAllCheckBox->isChecked())
        return vms::event::allEvents();

    QList<vms::api::EventType> result;
    for (vms::api::EventType eventType : m_businessRulesCheckBoxes.keys())
        if (m_businessRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    return result;
}

QSet<QnSystemHealth::MessageType> QnPopupSettingsWidget::watchedSystemHealth() const
{
    QSet<QnSystemHealth::MessageType> result;

    for (auto messageType: QnSystemHealth::allVisibleMessageTypes())
    {
        if (m_systemHealthCheckBoxes[messageType]->isChecked() || ui->showAllCheckBox->isChecked())
            result.insert(messageType);
    }

    return result;
}

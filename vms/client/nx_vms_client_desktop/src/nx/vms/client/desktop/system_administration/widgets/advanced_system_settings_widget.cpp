// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_system_settings_widget.h"
#include "advanced_system_settings_widget_p.h"

#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_administration/widgets/logs_management_widget.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/widgets/system_settings/database_management_widget.h>
#include <ui/workbench/workbench_context.h>

namespace {

const QUrl kBackupUrl{"#backup"};
const QUrl kLogsUrl{"#logs"};

} // namespace

namespace nx::vms::client::desktop {

QUrl AdvancedSystemSettingsWidget::Private::urlFor(Subpage page)
{
    switch (page)
    {
        case Subpage::backup:
            return kBackupUrl;

        case Subpage::logs:
            return kLogsUrl;

        default:
            NX_ASSERT(false, "");
            return {};
    }
}

AdvancedSystemSettingsWidget::Private::Private(AdvancedSystemSettingsWidget* q):
    QObject(q),
    q(q)
{
    m_menu = new QQuickWidget(qnClientCoreModule->mainQmlEngine(), q);
    m_menu->setClearColor(q->palette().window().color());
    m_menu->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_menu->setSource(QUrl("Nx/Dialogs/SystemSettings/AdvancedSettingsMenu.qml"));

    m_stack = new QStackedWidget(q);

    auto hBox = new QHBoxLayout();
    hBox->setContentsMargins(0, 0, 0, 0);
    hBox->setSpacing(0);

    auto vBox = new QVBoxLayout();
    vBox->setContentsMargins(16, 16, 16, 16);
    vBox->addWidget(m_stack);

    hBox->addWidget(m_menu);
    hBox->addLayout(vBox);
    q->setLayout(hBox);

    connect(m_menu->rootObject(), SIGNAL(currentIndexChanged(int)), this, SLOT(setCurrentTab(int)));
}

void AdvancedSystemSettingsWidget::Private::addTab(
    const QString& name, const QUrl& url, QWidget* widget)
{
    m_items << name;
    m_urls[url] = m_items.size() - 1; //< Items list has been updated already.
    m_menu->rootObject()->setProperty("items", m_items);
    m_stack->addWidget(widget);

    QMetaObject::invokeMethod(m_menu->rootObject(), "setCurrentIndex", Q_ARG(QVariant, 0));
}

bool AdvancedSystemSettingsWidget::Private::openSubpage(const QUrl& url)
{
    if (int idx = m_urls.value(url, -1);
        idx >= 0)
    {
        QMetaObject::invokeMethod(m_menu->rootObject(), "setCurrentIndex", Q_ARG(QVariant, idx));
    }

    return false;
}

void AdvancedSystemSettingsWidget::Private::setCurrentTab(int idx)
{
    m_stack->setCurrentIndex(idx);
}

bool AdvancedSystemSettingsWidget::Private::backupAndRestoreIsVisible() const
{
    const auto accessController = q->systemContext()->accessController();
    const auto isAdministrator =
        accessController->hasGlobalPermissions(GlobalPermission::administrator);
    const auto isPowerUser = accessController->hasGlobalPermissions(GlobalPermission::powerUser);

    const auto hasOwnerApiForAdmins = q->context()->currentServer()->getServerFlags().testFlag(
        nx::vms::api::SF_AdminApiForPowerUsers);

    return isAdministrator || (isPowerUser && hasOwnerApiForAdmins);
}

void AdvancedSystemSettingsWidget::Private::updateBackupAndRestoreTabVisibility()
{
    const bool visible = backupAndRestoreIsVisible();

    QMetaObject::invokeMethod(m_menu->rootObject(),
        "setIndexVisible",
        Q_ARG(QVariant, 0),
        Q_ARG(QVariant, visible));

    if (!visible && (m_stack->currentIndex() == (int)Subpage::backup))
    {
        const Subpage nextToBackupSubpage(Subpage((int)Subpage::backup + 1));
        openSubpage(urlFor(nextToBackupSubpage));
    }
}

QUrl AdvancedSystemSettingsWidget::urlFor(Subpage page)
{
    return Private::urlFor(page);
}

AdvancedSystemSettingsWidget::AdvancedSystemSettingsWidget(
    SystemContext* context,
    QWidget *parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    // Tabs must be added in the order they declared in AdvancedSystemSettingsWidget::Subpage.
    d->addTab(tr("Backup and Restore"), kBackupUrl, new QnDatabaseManagementWidget(this));
    d->addTab(tr("Logs Management"), kLogsUrl, new LogsManagementWidget(context, this));
}

AdvancedSystemSettingsWidget::~AdvancedSystemSettingsWidget()
{
}

bool AdvancedSystemSettingsWidget::activate(const QUrl& url)
{
    if (kBackupUrl == url && !d->backupAndRestoreIsVisible())
        return false;

    return d->openSubpage(url);
}

void AdvancedSystemSettingsWidget::loadDataToUi()
{
}

void AdvancedSystemSettingsWidget::applyChanges()
{
}

bool AdvancedSystemSettingsWidget::hasChanges() const
{
    return false;
}

void AdvancedSystemSettingsWidget::setReadOnlyInternal(bool readOnly)
{
}

void AdvancedSystemSettingsWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    d->updateBackupAndRestoreTabVisibility();
}

} // namespace nx::vms::client::desktop

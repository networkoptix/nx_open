// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_system_settings_widget.h"
#include "advanced_system_settings_widget_p.h"

#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

#include <client_core/client_core_module.h>
#include <nx/vms/client/desktop/system_administration/widgets/logs_management_widget.h>
#include <ui/widgets/system_settings/database_management_widget.h>

namespace {

const QUrl kBackupUrl{"#backup"};
const QUrl kLogsUrl{"#logs"};

} // namespace

namespace nx::vms::client::desktop {

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

AdvancedSystemSettingsWidget::AdvancedSystemSettingsWidget(
    SystemContext* context,
    QWidget *parent)
    :
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
    d->addTab(tr("Backup and Restore"), kBackupUrl, new QnDatabaseManagementWidget(this));
    d->addTab(tr("Logs Management"), kLogsUrl, new LogsManagementWidget(context, this));
}

AdvancedSystemSettingsWidget::~AdvancedSystemSettingsWidget()
{
}

bool AdvancedSystemSettingsWidget::activate(const QUrl& url)
{
    return d->openSubpage(url);
}

QUrl AdvancedSystemSettingsWidget::urlFor(Subpage page)
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

} // namespace nx::vms::client::desktop
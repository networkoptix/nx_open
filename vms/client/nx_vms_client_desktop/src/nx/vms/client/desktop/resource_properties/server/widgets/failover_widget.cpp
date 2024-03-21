// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_widget.h"
#include "ui_failover_widget.h"

#include <functional>

#include <api/server_rest_connection.h>
#include <camera/camera_data_manager.h>
#include <client/client_globals.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/rest/result.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>

namespace {

static const int kPcServerMaxCameras = 128;
static const int kArmServerMaxCameras = 12;
static const int kEdgeServerMaxCameras = 1;

} // namespace

namespace nx::vms::client::desktop {

struct FailoverWidget::Private
{
    QPointer<FailoverWidget> q;
    rest::Handle requestHandle = 0;
    QnMediaServerResourcePtr server;
    bool maxCamerasAdjusted;

public:
    void cancelRequests()
    {
        if (auto api = q->connectedServerApi())
        {
            if (requestHandle != 0)
            {
                api->cancelRequest(requestHandle);
                requestHandle = 0;
            }
        }
    }
};

FailoverWidget::FailoverWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::FailoverWidget),
    d(new Private{.q = this})
{
    ui->setupUi(this);

    ui->alertBar->init({.level = BarDescription::BarLevel::Error, .isOpenExternalLinks = false});

    setWarningStyle(ui->failoverWarningLabel);

    /* Set up context help. */
    setHelpTopic(ui->failoverGroupBox, HelpTopic::Id::ServerSettings_Failover);

    const auto failoverHint = HintButton::createGroupBoxHint(ui->failoverGroupBox);
    // Notice: this hint button uses help topic from the parent class.
    failoverHint->setHintText(
        tr("Servers with failover enabled will automatically take cameras from offline Servers "
           "with the same Location ID."));

    connect(ui->failoverGroupBox, &QGroupBox::toggled, this, [this](bool checked)
    {
        ui->maxCamerasWidget->setEnabled(checked);
        updateFailoverLabel();
        emit hasChangesChanged();
    });

    connect(ui->maxCamerasSpinBox, QnSpinboxIntValueChanged, this, [this]
    {
        d->maxCamerasAdjusted = true;
        updateFailoverLabel();
        emit hasChangesChanged();
    });

    connect(ui->locationIdSpinBox, QnSpinboxIntValueChanged, this,
        [this]
        {
            updateFailoverLabel();
            emit hasChangesChanged();
        });

    connect(ui->failoverPriorityButton, &QPushButton::clicked, action(menu::OpenFailoverPriorityAction), &QAction::trigger);

    retranslateUi();
}

FailoverWidget::~FailoverWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

QnMediaServerResourcePtr FailoverWidget::server() const
{
    return d->server;
}

void FailoverWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (d->server == server)
        return;

    if (d->server)
        d->server->disconnect(this);

    d->server = server;

    if (d->server)
    {
        connect(d->server.get(), &QnMediaServerResource::redundancyChanged, this,
            &FailoverWidget::updateFailoverSettings);
        connect(d->server.get(), &QnMediaServerResource::maxCamerasChanged, this,
            [this]()
            {
                updateMaxCamerasSettings();
                d->maxCamerasAdjusted = true;
            });
        connect(d->server.get(), &QnMediaServerResource::locationIdChanged,
            this, &FailoverWidget::updateLocationIdSettings);
    }

    updateReadOnly();
}

void FailoverWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;
    setReadOnly(ui->failoverGroupBox, readOnly);
    setReadOnly(ui->maxCamerasSpinBox, readOnly);
    setReadOnly(ui->locationIdSpinBox, readOnly);
}

void FailoverWidget::updateReadOnly()
{
    bool readOnly = [&]()
    {
        if (!d->server)
            return true;

        return !systemContext()->accessController()->hasPermissions(
            d->server, Qn::WritePermission | Qn::SavePermission);
    }();

    setReadOnly(readOnly);
}

bool FailoverWidget::hasChanges() const
{
    if (!d->server)
        return false;

    if (isReadOnly())
        return false;

    if (d->server->isRedundancy() != ui->failoverGroupBox->isChecked())
        return true;

    if (d->server->isRedundancy())
    {
        if (d->server->getMaxCameras() != ui->maxCamerasSpinBox->value())
            return true;
        if (d->server->locationId() != ui->locationIdSpinBox->value())
            return true;
    }

    return false;
}

void FailoverWidget::retranslateUi()
{
    ui->failoverGroupBox->setTitle(tr("Failover"));

    ui->maxCamerasLabel->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Max devices on this server:"),
        tr("Max cameras on this server:")
    ));

    updateFailoverLabel();
}

bool FailoverWidget::canApplyChanges() const
{
    return true;
}

bool FailoverWidget::isNetworkRequestRunning() const
{
    return d->requestHandle != 0;
}

void FailoverWidget::updateMaxCamerasSettings()
{
    if (!d->server)
        return;

    int maxCameras;
    if (d->server->getServerFlags().testFlag(vms::api::SF_Edge))
        maxCameras = kEdgeServerMaxCameras;   //edge server
    else if (d->server->getServerFlags().testFlag(vms::api::SF_ArmServer))
        maxCameras = kArmServerMaxCameras;   //generic ARM based servre
    else
        maxCameras = kPcServerMaxCameras;    //PC server
    ui->maxCamerasSpinBox->setMaximum(maxCameras);

    int currentMaxCamerasValue = d->server->getMaxCameras();
    if (currentMaxCamerasValue == 0)
        currentMaxCamerasValue = maxCameras;

    ui->maxCamerasSpinBox->setValue(currentMaxCamerasValue);
    ui->maxCamerasWidget->setEnabled(d->server->isRedundancy());
}

void FailoverWidget::updateLocationIdSettings()
{
    if (!d->server)
        return;

    ui->locationIdSpinBox->setValue(d->server->locationId());
}

void FailoverWidget::updateFailoverSettings()
{
    if (!d->server)
        return;

    updateMaxCamerasSettings();
    updateLocationIdSettings();

    ui->failoverGroupBox->setChecked(d->server->isRedundancy());
    updateFailoverLabel();
}

void FailoverWidget::loadDataToUi()
{
    if (!d->server)
        return;

    updateFailoverSettings();
    d->maxCamerasAdjusted = false;
}

void FailoverWidget::applyChanges()
{
    if (!d->server)
        return;

    if (isReadOnly())
        return;

    d->server->setMaxCameras(ui->maxCamerasSpinBox->value());
    d->server->setLocationId(ui->locationIdSpinBox->value());
    d->server->setRedundancy(ui->failoverGroupBox->isChecked());

    auto callback = nx::utils::guarded(this,
        [this](bool /*success*/, rest::Handle requestId)
        {
            if (NX_ASSERT(requestId == d->requestHandle || d->requestHandle == 0))
                d->requestHandle = 0;
        });

    d->requestHandle = qnResourcesChangesManager->saveServer(d->server, callback);
}

void FailoverWidget::discardChanges()
{
    d->cancelRequests();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void FailoverWidget::updateFailoverLabel()
{
    auto getErrorText = [this]
    {
        if (!d->server)
            return QString();

        if (resourcePool()->getResources<QnMediaServerResource>().size() < 2)
            return tr("At least two servers are required for this feature.");

        if (resourcePool()->getAllCameras(d->server, true).size() > ui->maxCamerasSpinBox->value())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("This server already has more than max devices"),
                tr("This server already has more than max cameras")
            );

        if (!d->server->isRedundancy() && !d->maxCamerasAdjusted)
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("To avoid issues adjust max number of devices"),
                tr("To avoid issues adjust max number of cameras")
            );

        return QString();
    };

    QString error;
    if (ui->failoverGroupBox->isChecked())
        error = getErrorText();

    ui->failoverWarningLabel->setText(error);
    ui->failoverWarningLabel->setHidden(error.isEmpty());
}

} // namespace nx::vms::client::desktop

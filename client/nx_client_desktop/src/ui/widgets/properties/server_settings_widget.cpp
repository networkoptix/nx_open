#include "server_settings_widget.h"
#include "ui_server_settings_widget.h"

#include <functional>

#include <QtCore/QDir>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLabel>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <api/app_server_connection.h>

#include <camera/camera_data_manager.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>

#include <client/client_model_types.h>
#include <client/client_settings.h>

#include <network/cloud_url_validator.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <nx/utils/uuid.h>
#include <nx/utils/counter.h>
#include <nx/utils/string.h>
#include <utils/common/variant.h>
#include <utils/common/event_processors.h>
#include <utils/math/interpolator.h>
#include <utils/math/color_transformations.h>

using namespace nx::client::desktop::ui;

namespace {
static const int PC_SERVER_MAX_CAMERAS = 128;
static const int ARM_SERVER_MAX_CAMERAS = 12;
static const int EDGE_SERVER_MAX_CAMERAS = 1;
}


//#define QN_SHOW_ARCHIVE_SPACE_COLUMN

QnServerSettingsWidget::QnServerSettingsWidget(QWidget* parent /* = 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::ServerSettingsWidget)
    , m_server()
    , m_tableBottomLabel()
    , m_maxCamerasAdjusted(false)
    , m_rebuildWasCanceled(false)
    , m_initServerName()
{
    ui->setupUi(this);

    setWarningStyle(ui->failoverWarningLabel);

    /* Set up context help. */
    setHelpTopic(ui->nameLabel, ui->nameLineEdit, Qn::ServerSettings_General_Help);
    setHelpTopic(ui->nameLabel, ui->maxCamerasSpinBox, Qn::ServerSettings_General_Help);
    setHelpTopic(ui->ipAddressLabel, ui->ipAddressLineEdit, Qn::ServerSettings_General_Help);
    setHelpTopic(ui->portLabel, ui->portLineEdit, Qn::ServerSettings_General_Help);
    setHelpTopic(ui->failoverGroupBox, Qn::ServerSettings_Failover_Help);

    connect(ui->pingButton, &QPushButton::clicked, this, &QnServerSettingsWidget::at_pingButton_clicked);

    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->failoverGroupBox, &QGroupBox::toggled, this, [this](bool checked)
    {
        ui->maxCamerasWidget->setEnabled(checked);
        updateFailoverLabel();
        emit hasChangesChanged();
    });


    connect(ui->maxCamerasSpinBox, QnSpinboxIntValueChanged, this, [this]
    {
        m_maxCamerasAdjusted = true;
        updateFailoverLabel();
        emit hasChangesChanged();
    });

    connect(ui->failoverPriorityButton, &QPushButton::clicked, action(action::OpenFailoverPriorityAction), &QAction::trigger);

    retranslateUi();
}

QnServerSettingsWidget::~QnServerSettingsWidget()
{}

QnMediaServerResourcePtr QnServerSettingsWidget::server() const
{
    return m_server;
}

void QnServerSettingsWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server)
        return;

    if (m_server)
    {
        disconnect(m_server, nullptr, this, nullptr);
    }

    m_server = server;

    if (m_server)
    {
        connect(m_server, &QnResource::nameChanged, this, [this](const QnResourcePtr &resource)
        {
            if (ui->nameLineEdit->text() != m_initServerName)   /// Field was changed
                return;

            m_initServerName = resource->getName();
            ui->nameLineEdit->setText(m_initServerName);
        });

        connect(m_server, &QnResource::urlChanged, this, [this](const QnResourcePtr &resource)
        {
            Q_UNUSED(resource);
            updateUrl();
        });
    }

    updateReadOnly();
}

void QnServerSettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;
    setReadOnly(ui->failoverGroupBox, readOnly);
    setReadOnly(ui->maxCamerasSpinBox, readOnly);

}

void QnServerSettingsWidget::updateReadOnly()
{
    bool readOnly = [&]()
    {
        if (!m_server)
            return true;
        return !accessController()->hasPermissions(m_server, Qn::WritePermission | Qn::SavePermission);
    }();

    setReadOnly(readOnly);

    /* Edge servers cannot be renamed. */
    ui->nameLineEdit->setReadOnly(readOnly || QnMediaServerResource::isEdgeServer(m_server));
}

bool QnServerSettingsWidget::hasChanges() const
{
    if (!m_server)
        return false;

    if (isReadOnly())
        return false;

    if (m_server->getName() != ui->nameLineEdit->text())
        return true;

    if (m_server->isRedundancy() != ui->failoverGroupBox->isChecked())
        return true;

    if (m_server->isRedundancy() &&
        (m_server->getMaxCameras() != ui->maxCamerasSpinBox->value()))
        return true;

    return false;
}

void QnServerSettingsWidget::retranslateUi()
{
    QString failoverText = QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("server will take devices automatically from offline servers"),
        tr("server will take cameras automatically from offline servers"));

    ui->failoverGroupBox->setTitle(tr("Failover") + lit("\t(%1)").arg(failoverText));

    ui->maxCamerasLabel->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Max devices on this server:"),
        tr("Max cameras on this server:")
    ));

    updateFailoverLabel();
}

bool QnServerSettingsWidget::canApplyChanges() const
{
    return !ui->nameLineEdit->text().trimmed().isEmpty();
}

void QnServerSettingsWidget::loadDataToUi()
{
    if (!m_server)
        return;

    m_initServerName = m_server->getName();
    ui->nameLineEdit->setText(m_initServerName);
    int maxCameras;
    if (m_server->getServerFlags().testFlag(Qn::SF_Edge))
        maxCameras = EDGE_SERVER_MAX_CAMERAS;   //edge server
    else if (m_server->getServerFlags().testFlag(Qn::SF_ArmServer))
        maxCameras = ARM_SERVER_MAX_CAMERAS;   //generic ARM based servre
    else
        maxCameras = PC_SERVER_MAX_CAMERAS;    //PC server
    ui->maxCamerasSpinBox->setMaximum(maxCameras);

    int currentMaxCamerasValue = m_server->getMaxCameras();
    if (currentMaxCamerasValue == 0)
        currentMaxCamerasValue = maxCameras;

    ui->maxCamerasSpinBox->setValue(currentMaxCamerasValue);
    ui->failoverGroupBox->setChecked(m_server->isRedundancy());
    ui->maxCamerasWidget->setEnabled(m_server->isRedundancy());

    m_maxCamerasAdjusted = false;

    updateFailoverLabel();
    updateUrl();
}

void QnServerSettingsWidget::applyChanges()
{
    if (!m_server)
        return;

    if (isReadOnly())
        return;

    qnResourcesChangesManager->saveServer(m_server, [this](const QnMediaServerResourcePtr &server)
    {
        server->setName(ui->nameLineEdit->text());
        server->setMaxCameras(ui->maxCamerasSpinBox->value());
        server->setRedundancy(ui->failoverGroupBox->isChecked());
    });
}

void QnServerSettingsWidget::updateUrl()
{
    if (!m_server)
        return;

    auto displayInfo = QnResourceDisplayInfo(m_server);
    ui->ipAddressLineEdit->setText(displayInfo.host());
    ui->portLineEdit->setText(QString::number(displayInfo.port()));

    ui->pingButton->setVisible(!nx::network::isCloudServer(m_server));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnServerSettingsWidget::updateFailoverLabel()
{

    auto getErrorText = [this]
    {
        if (!m_server)
            return QString();

        if (resourcePool()->getResources<QnMediaServerResource>().size() < 2)
            return tr("At least two servers are required for this feature.");

        if (resourcePool()->getAllCameras(m_server, true).size() > ui->maxCamerasSpinBox->value())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("This server already has more than max devices"),
                tr("This server already has more than max cameras")
            );

        if (!m_server->isRedundancy() && !m_maxCamerasAdjusted)
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


void QnServerSettingsWidget::at_pingButton_clicked()
{
    if (!m_server)
        return;

    /* We must always ping the same address that is displayed in the visible field. */
    menu()->trigger(action::PingAction, {Qn::TextRole, ui->ipAddressLineEdit->text()});
}

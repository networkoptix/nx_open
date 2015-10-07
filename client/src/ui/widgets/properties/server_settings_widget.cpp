#include "server_settings_widget.h"
#include "ui_server_settings_widget.h"

#include <functional>

#include <QtCore/QDir>
#include <QtWidgets/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLabel>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <api/app_server_connection.h>

#include <core/resource/resource_name.h>
#include <camera/camera_data_manager.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>

#include <client/client_model_types.h>
#include <client/client_settings.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/noptix_style.h>
#include <ui/style/warning_style.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/uuid.h>
#include <utils/common/counter.h>
#include <utils/common/string.h>
#include <utils/common/variant.h>
#include <utils/common/event_processors.h>
#include <utils/math/interpolator.h>
#include <utils/math/color_transformations.h>

namespace
{
    static const int PC_SERVER_MAX_CAMERAS = 128;
    static const int ARM_SERVER_MAX_CAMERAS = 12;
    static const int EDGE_SERVER_MAX_CAMERAS = 1;
}


//#define QN_SHOW_ARCHIVE_SPACE_COLUMN

QnServerSettingsWidget::QnServerSettingsWidget(QWidget* parent /* = 0*/):
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
    setHelpTopic(ui->nameLabel,           ui->nameLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->nameLabel,           ui->maxCamerasSpinBox,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->ipAddressLabel,      ui->ipAddressLineEdit,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->portLabel,           ui->portLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->failoverGroupBox,                                        Qn::ServerSettings_Failover_Help);

    connect(ui->pingButton,             &QPushButton::clicked,      this,   &QnServerSettingsWidget::at_pingButton_clicked);
    
    connect(ui->failoverCheckBox,       &QCheckBox::stateChanged,       this,   [this] {
        ui->maxCamerasWidget->setEnabled(ui->failoverCheckBox->isChecked());
        updateFailoverLabel();
    });

    
    connect(ui->maxCamerasSpinBox,      QnSpinboxIntValueChanged,       this,   [this] {
        m_maxCamerasAdjusted = true;
        updateFailoverLabel();
    });

    connect(ui->failoverPriorityButton, &QPushButton::clicked,  action(Qn::OpenFailoverPriorityAction), &QAction::trigger);

    retranslateUi();
}

QnServerSettingsWidget::~QnServerSettingsWidget()
{}

QnMediaServerResourcePtr QnServerSettingsWidget::server() const {
    return m_server;
}

void QnServerSettingsWidget::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
        return;

    if (m_server) {
        disconnect(m_server, nullptr, this, nullptr);
    }

    m_server = server;

    if (m_server) {
        connect(m_server, &QnResource::nameChanged, this, [this](const QnResourcePtr &resource)
        {
            if (ui->nameLineEdit->text() != m_initServerName)   /// Field was changed
                return;

            m_initServerName = resource->getName();
            ui->nameLineEdit->setText(m_initServerName);
        });
    }

    updateReadOnly();
}

void QnServerSettingsWidget::setReadOnlyInternal(bool readOnly) {
    using ::setReadOnly;
    setReadOnly(ui->failoverCheckBox, readOnly);
    setReadOnly(ui->maxCamerasSpinBox, readOnly);

}

void QnServerSettingsWidget::updateReadOnly() {
    bool readOnly = [&](){
        if (!m_server)
            return true;
        return !accessController()->hasPermissions(m_server, Qn::WritePermission | Qn::SavePermission);
    }();

    setReadOnly(readOnly);

    /* Edge servers cannot be renamed. */
    ui->nameLineEdit->setReadOnly(readOnly || QnMediaServerResource::isEdgeServer(m_server));
}

bool QnServerSettingsWidget::hasChanges() const {
    if (isReadOnly())
        return false;

    if (m_server->getName() != ui->nameLineEdit->text())
        return true;

    if (m_server->isRedundancy() != ui->failoverCheckBox->isChecked())
        return true;

    if (m_server->isRedundancy() && 
        (m_server->getMaxCameras() != ui->maxCamerasSpinBox->value()))
        return true;

    return false;
}

void QnServerSettingsWidget::retranslateUi() {
    ui->failoverCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Enable failover (server will take devices automatically from offline servers)"),
        tr("Enable failover (server will take cameras automatically from offline servers)")
        ));
        
    ui->maxCamerasLabel->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Max devices on this server:"),
        tr("Max cameras on this server:")
        ));
        
    updateFailoverLabel();
}

void QnServerSettingsWidget::updateFromSettings() {

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
    if( currentMaxCamerasValue == 0 )
        currentMaxCamerasValue = maxCameras;

    ui->maxCamerasSpinBox->setValue(currentMaxCamerasValue);
    ui->failoverCheckBox->setChecked(m_server->isRedundancy());
    ui->maxCamerasWidget->setEnabled(m_server->isRedundancy());   

    ui->ipAddressLineEdit->setText(QUrl(m_server->getUrl()).host());
    ui->portLineEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    m_maxCamerasAdjusted = false;

    updateFailoverLabel();
}

void QnServerSettingsWidget::submitToSettings() {
    if (isReadOnly())
        return;

    qnResourcesChangesManager->saveServer(m_server, [this](const QnMediaServerResourcePtr &server) {
        server->setName(ui->nameLineEdit->text());
        server->setMaxCameras(ui->maxCamerasSpinBox->value());
        server->setRedundancy(ui->failoverCheckBox->isChecked());
    });


}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnServerSettingsWidget::updateFailoverLabel() {

    auto getErrorText = [this] {
        if (qnResPool->getResources<QnMediaServerResource>().size() < 2)
            return tr("At least two servers are required for this feature.");

        if (qnResPool->getAllCameras(m_server, true).size() > ui->maxCamerasSpinBox->value())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("This server already has more than max devices"),
                tr("This server already has more than max cameras")
            );

        if (!m_server->isRedundancy() && !m_maxCamerasAdjusted)
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("To avoid malfunction adjust max number of devices"),
                tr("To avoid malfunction adjust max number of cameras")
            );

        return QString();
    };

    QString error;
    if (ui->failoverCheckBox->isChecked())
        error = getErrorText();

    ui->failoverWarningLabel->setText(error);
}


void QnServerSettingsWidget::at_pingButton_clicked() {
    menu()->trigger(Qn::PingAction, QnActionParameters(m_server));
}


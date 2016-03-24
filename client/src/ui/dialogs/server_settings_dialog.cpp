#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <core/resource/media_server_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/properties/server_settings_widget.h>
#include <ui/widgets/properties/recording_statistics_widget.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include "ui/widgets/properties/storage_config_widget.h"
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

QnServerSettingsDialog::QnServerSettingsDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::ServerSettingsDialog)
    , m_server()
    , m_generalPage(new QnServerSettingsWidget(this))
    , m_statisticsPage(new QnRecordingStatisticsWidget(this))
    , m_storagesPage(new QnStorageConfigWidget(this))
    , m_webPageButton(new QPushButton(tr("Open Web Page..."), this))
{
    ui->setupUi(this);

    addPage(SettingsPage, m_generalPage, tr("General"));
    addPage(StorageManagmentPage, m_storagesPage, tr("Storage Management"));
    addPage(StatisticsPage, m_statisticsPage, tr("Storage Analytics"));

    /* Handling scenario when user closed the dialog and reopened it again. */
    connect(this, &QnGenericTabbedDialog::dialogClosed, m_statisticsPage, &QnRecordingStatisticsWidget::resetForecast);

    connect(m_webPageButton, &QPushButton::clicked, this, [this] {
        menu()->trigger(QnActions::WebClientAction, m_server);
    });


    ui->buttonBox->addButton(m_webPageButton, QDialogButtonBox::HelpRole);
    setHelpTopic(m_webPageButton, Qn::ServerSettings_WebClient_Help);

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources) {
        if (isHidden())
            return;

        auto servers = resources.filtered<QnMediaServerResource>([](const QnMediaServerResourcePtr &server){
            return !QnMediaServerResource::isFakeServer(server);
        });

        if (!servers.isEmpty())
            setServer(servers.first());
    });  

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);   
}

QnServerSettingsDialog::~QnServerSettingsDialog() 
{}

QnMediaServerResourcePtr QnServerSettingsDialog::server() const {
    return m_server;
}

void QnServerSettingsDialog::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
        return;

    if (!tryClose(false))
        return;

    m_generalPage->setServer(server);
    m_statisticsPage->setServer(server);
    m_storagesPage->setServer(server);

    if (m_server)
        disconnect(m_server, nullptr, this, nullptr);

    m_server = server;

    m_webPageButton->setEnabled(server && server->getStatus() == Qn::Online);

    if (m_server) {
        connect(m_server, &QnResource::statusChanged, this, [this] {
            m_webPageButton->setEnabled(m_server->getStatus() == Qn::Online);
        });
    }

    loadDataToUi();
}

void QnServerSettingsDialog::retranslateUi() {
    base_type::retranslateUi();
    if (m_server) {
        bool readOnly = !accessController()->hasPermissions(m_server, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("Server Settings - %1 (readonly)").arg(m_server->getName())
            : tr("Server Settings - %1").arg(m_server->getName())
            );

    } else {
        setWindowTitle(tr("Server Settings"));
    }
}

QMessageBox::StandardButton QnServerSettingsDialog::showConfirmationDialog() {
    NX_ASSERT(m_server, Q_FUNC_INFO, "Server must exist here");

    return QMessageBox::question(this,
        tr("Server not saved"),
        tr("Apply changes to server %1?")
        .arg(m_server
            ? m_server->getName()
            : QString()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
}

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
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

QnServerSettingsDialog::QnServerSettingsDialog(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::ServerSettingsDialog)
    , m_server()
    , m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnServerSettingsDialog>(this))
    , m_generalPage(new QnServerSettingsWidget(this))
    , m_statisticsPage(new QnRecordingStatisticsWidget(this))
    , m_webPageButton(new QPushButton(tr("Open Web Page..."), this))
{
    ui->setupUi(this);
  
    addPage(SettingsPage, m_generalPage, tr("General"));
    addPage(StatisticsPage, m_statisticsPage, tr("Storage Analytics"));


    connect(m_webPageButton, &QPushButton::clicked, this, [this] {
        menu()->trigger(Qn::WebClientAction, m_server);
    });


    ui->buttonBox->addButton(m_webPageButton, QDialogButtonBox::HelpRole);
    setHelpTopic(m_webPageButton, Qn::ServerSettings_WebClient_Help);

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources) {
        if (isHidden())
            return;

        auto servers = resources.filtered<QnMediaServerResource>([](const QnMediaServerResourcePtr &server){
            return server->getStatus() != Qn::Incompatible;
        });

        if (!servers.isEmpty())
            setServer(servers.first());
    });  

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);
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

    if (m_server)
        disconnect(m_server, nullptr, this, nullptr);

    m_server = server;

    m_webPageButton->setEnabled(server && server->getStatus() == Qn::Online);

    if (m_server) {
        connect(m_server, &QnResource::statusChanged, this, [this] {
            m_webPageButton->setEnabled(m_server->getStatus() == Qn::Online);
        });
    }

    loadData();
}

void QnServerSettingsDialog::loadData() {
    base_type::loadData();
    
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

QString QnServerSettingsDialog::confirmMessageTitle() const {
    return tr("Server not saved");
}

QString QnServerSettingsDialog::confirmMessageText() const {
    Q_ASSERT_X(m_server, Q_FUNC_INFO, "Server must exist here");
    return tr("Apply changes to the server %1?").arg(m_server
        ? m_server->getName()
        : QString());    
}

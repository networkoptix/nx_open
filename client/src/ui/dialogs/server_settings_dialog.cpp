#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <core/resource/media_server_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/properties/server_settings_widget.h>
#include <ui/widgets/properties/recording_statistics_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

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
}

QnServerSettingsDialog::~QnServerSettingsDialog() 
{}

bool QnServerSettingsDialog::tryClose(bool force)
{
    if (!base_type::tryClose(force))
        return false;

    return true;
}

QnMediaServerResourcePtr QnServerSettingsDialog::server() const {
    return m_server;
}

void QnServerSettingsDialog::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
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



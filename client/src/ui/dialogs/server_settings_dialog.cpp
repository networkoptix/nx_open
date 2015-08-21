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

QnServerSettingsDialog::QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnServerSettingsDialog>(this))
{
    ui->setupUi(this);
  
    addPage(SettingsPage, new QnServerSettingsWidget(server, this), tr("General"));
    addPage(StatisticsPage, new QnRecordingStatisticsWidget(server, this), tr("Statistics"));

    QPushButton* webPageButton = new QPushButton(this);
    webPageButton->setText(tr("Open Web Page..."));
    webPageButton->setEnabled(m_server->getStatus() == Qn::Online);

    connect(m_server, &QnResource::statusChanged, this, [this, webPageButton] {
        webPageButton->setEnabled(m_server->getStatus() == Qn::Online);
    });

    connect(webPageButton, &QPushButton::clicked, this, [this] {
        menu()->trigger(Qn::WebClientAction);
    });


    ui->buttonBox->addButton(webPageButton, QDialogButtonBox::HelpRole);
    setHelpTopic(webPageButton, Qn::ServerSettings_WebClient_Help);

    loadData();
}

QnServerSettingsDialog::~QnServerSettingsDialog() 
{}

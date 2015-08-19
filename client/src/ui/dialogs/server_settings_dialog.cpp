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
#include <ui/dialogs/non_modal_dialog_constructor.h>

namespace
{
    typedef QHash<QnUuid, QnServerSettingsDialog *> DialogsContainer;

    DialogsContainer g_currentShowingDialogs;
}

void QnServerSettingsDialog::showNonModal(const QnMediaServerResourcePtr &server
    , const AcceptCallback &callback
    , QWidget *parent)
{
    auto it = std::find_if(g_currentShowingDialogs.begin(), g_currentShowingDialogs.end()
        , [server](QnServerSettingsDialog *dialog)
    {
        return (dialog->m_server->getId() == server->getId());
    });

    QnServerSettingsDialog *dlg = nullptr;
    if (it == g_currentShowingDialogs.end())
        it = g_currentShowingDialogs.insert(server->getId(), new QnServerSettingsDialog(server, callback, parent));

    QnShowDialogHelper::showNonModalDialog(it.value());
}

QnServerSettingsDialog::QnServerSettingsDialog(const QnMediaServerResourcePtr &server
    , const AcceptCallback &callback
    , QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::ServerSettingsDialog)
    , m_onAcceptClickedCallback(callback)
    , m_server(server)
    , m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnServerSettingsDialog>(this))
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
{
}

void QnServerSettingsDialog::reject()
{
    g_currentShowingDialogs.remove(m_server->getId());
    base_type::reject();
    deleteLater();
}

void QnServerSettingsDialog::accept()
{
    g_currentShowingDialogs.remove(m_server->getId());
    base_type::accept();

    if (m_onAcceptClickedCallback)
        m_onAcceptClickedCallback();

    deleteLater();
}


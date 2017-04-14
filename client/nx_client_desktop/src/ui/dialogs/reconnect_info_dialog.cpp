#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/common/progress_widget.h>

namespace {
const int maxLabelWidth = 400;
}

QnReconnectInfoDialog::QnReconnectInfoDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ReconnectInfoDialog())
{
    ui->setupUi(this);

    if (!qnRuntime->isDesktopMode())
        ui->buttonBox->setVisible(false);
}

QnReconnectInfoDialog::~QnReconnectInfoDialog()
{
}

QnMediaServerResourcePtr QnReconnectInfoDialog::currentServer() const
{
    return m_currentServer;
}

void QnReconnectInfoDialog::setCurrentServer(const QnMediaServerResourcePtr &server)
{
    if (m_currentServer == server)
        return;
    m_currentServer = server;
    emit currentServerChanged(server);
}

void QnReconnectInfoDialog::reject()
{
    if (!qnRuntime->isDesktopMode())
        return;
    base_type::reject();
}

QnMediaServerResourceList QnReconnectInfoDialog::servers() const
{
    return m_servers;
}

void QnReconnectInfoDialog::setServers(const QnMediaServerResourceList &servers)
{
    m_servers = servers;
    int row = 0;
    for (const auto server : m_servers)
    {
        QString text = QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());

        QLabel* nameLabel = new QLabel(this);
        nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, maxLabelWidth));

        QLabel *iconLabel = new QLabel(this);
        iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(18, 18));

        auto progressWidget = new QnProgressWidget(this);
        connect(this, &QnReconnectInfoDialog::currentServerChanged, this,
            [this, progressWidget, nameLabel, server]
            (const QnMediaServerResourcePtr &currentServer)
            {
                progressWidget->setVisible(server == currentServer);
                QFont baseFont = this->font();
                baseFont.setBold(server == currentServer);
                nameLabel->setFont(baseFont);
            });
        progressWidget->setVisible(server == m_currentServer);

        ui->gridLayout->addWidget(progressWidget, row, 0);
        ui->gridLayout->addWidget(iconLabel, row, 1);
        ui->gridLayout->addWidget(nameLabel, row, 2);
        ++row;

        if (row == 1)
            ui->gridLayout->setColumnStretch(2, 1);
    }

}

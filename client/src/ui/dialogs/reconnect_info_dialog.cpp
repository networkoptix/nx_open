#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/progress_widget.h>

#include <utils/common/uuid.h>
#include <utils/common/string.h>

QnReconnectInfoDialog::QnReconnectInfoDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ReconnectInfoDialog()),
    m_cancelled(false)
{
    ui->setupUi(this);

    connect(ui->buttonBox,  &QDialogButtonBox::rejected, this, [this]{
        m_cancelled = true;
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
        ui->label->setText(tr("Canceling..."));
    });

    QnUuid currentServerId = qnCommon->remoteGUID();
    for (const QnMediaServerResourcePtr &server: qnResPool->getResources<QnMediaServerResource>()) {
        m_servers << server;
        if (server->getId() == currentServerId)
            m_currentServer = server;
    }

    qSort(m_servers.begin(), m_servers.end(), [](const QnMediaServerResourcePtr &left, const QnMediaServerResourcePtr &right) {
        return naturalStringCompare(getResourceName(left), getResourceName(right), Qt::CaseInsensitive , false);
    });

    int row = 0;
    for (const QnMediaServerResourcePtr server: m_servers) {
        QLabel* nameLabel = new QLabel(this);
        nameLabel->setText(getResourceName(server));

        QLabel *iconLabel = new QLabel(this);
        iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(24, 24));

        QnProgressWidget* progressWidget = new QnProgressWidget(this);
        connect(this, &QnReconnectInfoDialog::currentServerChanged, this, [progressWidget, nameLabel, server](const QnMediaServerResourcePtr &currentServer) {
            progressWidget->setVisible(server == currentServer);
            if (server == currentServer) 
                nameLabel->setText(lit("<b>%1</b>").arg(getResourceName(server)));
            else 
                nameLabel->setText(getResourceName(server));
        });
        progressWidget->setVisible(server == m_currentServer);

        ui->gridLayout->addWidget(iconLabel, row, 0);
        ui->gridLayout->addWidget(nameLabel, row, 1);
        ui->gridLayout->addWidget(progressWidget, row, 2);
        ++row;

        if (row == 1)
            ui->gridLayout->setColumnStretch(1, 1);
    }
}

QnReconnectInfoDialog::~QnReconnectInfoDialog()
{}

bool QnReconnectInfoDialog::wasCanceled() const {
    return m_cancelled;
}

QnMediaServerResourcePtr QnReconnectInfoDialog::currentServer() const {
    return m_currentServer;
}

void QnReconnectInfoDialog::setCurrentServer(const QnMediaServerResourcePtr &server) {
    if (m_currentServer == server)
        return;
    m_currentServer = server;
    emit currentServerChanged(server);
}

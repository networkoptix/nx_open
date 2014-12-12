#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

#include <core/resource/media_server_resource.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/progress_widget.h>

namespace {
    const int maxLabelWidth = 400;
}

QnReconnectInfoDialog::QnReconnectInfoDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ReconnectInfoDialog()),
    m_cancelled(false)
{
    ui->setupUi(this);

    connect(ui->buttonBox,  &QDialogButtonBox::rejected, this, [this]{
        reject();
    });
}

QnReconnectInfoDialog::~QnReconnectInfoDialog()
{}


void QnReconnectInfoDialog::reject() {
    m_cancelled = true;
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
    ui->label->setText(tr("Canceling..."));
    /* We are not closing dialog here intentionally. */
}


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

QnMediaServerResourceList QnReconnectInfoDialog::servers() const {
    return m_servers;
}

void QnReconnectInfoDialog::setServers(const QnMediaServerResourceList &servers) {
    m_servers = servers;
    int row = 0;
    for (const QnMediaServerResourcePtr server: m_servers) {
        QString text = getResourceName(server);

        QLabel* nameLabel = new QLabel(this);
        nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, maxLabelWidth));

        QLabel *iconLabel = new QLabel(this);
        iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(18, 18));

        QnProgressWidget* progressWidget = new QnProgressWidget(this);
        connect(this, &QnReconnectInfoDialog::currentServerChanged, this, [this, progressWidget, nameLabel, server](const QnMediaServerResourcePtr &currentServer) {
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

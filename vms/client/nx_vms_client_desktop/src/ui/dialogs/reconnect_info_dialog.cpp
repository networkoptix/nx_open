// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/widgets/common/progress_widget.h>

namespace {
const int maxLabelWidth = 400;
}

QnReconnectInfoDialog::QnReconnectInfoDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ReconnectInfoDialog())
{
    ui->setupUi(this);

    if (qnRuntime->isVideoWallMode())
        ui->buttonBox->setVisible(false);
}

QnReconnectInfoDialog::~QnReconnectInfoDialog()
{
}

QnMediaServerResourcePtr QnReconnectInfoDialog::currentServer() const
{
    return m_currentServer;
}

void QnReconnectInfoDialog::setCurrentServer(const QnMediaServerResourcePtr& server)
{
    if (m_currentServer == server)
        return;
    m_currentServer = server;
    QString text = QnResourceDisplayInfo(server).toString(qnSettings->resourceInfoLevel());

    ui->nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, maxLabelWidth));
    ui->iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(18, 18));

    emit currentServerChanged(server);
}

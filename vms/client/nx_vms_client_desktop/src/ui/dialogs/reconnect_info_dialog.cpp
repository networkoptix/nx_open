// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

#include <client/client_runtime_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/widgets/common/progress_widget.h>

using namespace nx::vms::client::desktop;

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
    using nx::vms::client::core::ResourceIconCache;

    if (m_currentServer == server)
        return;
    m_currentServer = server;
    QString text = QnResourceDisplayInfo(server).toString(
        appContext()->localSettings()->resourceInfoLevel());

    ui->nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, maxLabelWidth));
    ui->iconLabel->setPixmap(
        appContext()->resourceIconCache()->icon(ResourceIconCache::Server).pixmap(18, 18));

    emit currentServerChanged(server);
}

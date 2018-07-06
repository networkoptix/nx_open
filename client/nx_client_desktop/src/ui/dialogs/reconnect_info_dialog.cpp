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

void QnReconnectInfoDialog::setCurrentServer(const QnMediaServerResourcePtr& server)
{
    if (m_currentServer == server)
        return;
    m_currentServer = server;
    QString text = QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());

    ui->nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, maxLabelWidth));
    ui->iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(18, 18));

    emit currentServerChanged(server);
}

void QnReconnectInfoDialog::reject()
{
    if (!qnRuntime->isDesktopMode())
        return;
    base_type::reject();
}

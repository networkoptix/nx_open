#include "connecting_to_server_dialog.h"
#include "ui_connecting_to_server_dialog.h"

#include <client/client_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <ui/style/resource_icon_cache.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxLabelWidth = 400;

} // namespace

ConnectingToServerDialog::ConnectingToServerDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ConnectingToServerDialog())
{
    ui->setupUi(this);
    ui->nameLabel->setText("");
    ui->iconLabel->setText("");
}

ConnectingToServerDialog::~ConnectingToServerDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}

void ConnectingToServerDialog::setDisplayedServer(const QnMediaServerResourcePtr& server)
{
    if (server)
    {
        const auto text = QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());
        ui->nameLabel->setText(fontMetrics().elidedText(text, Qt::ElideMiddle, kMaxLabelWidth));
        ui->iconLabel->setPixmap(qnResIconCache->icon(QnResourceIconCache::Server).pixmap(18, 18));
    }
    else
    {
        ui->nameLabel->setText("");
        ui->iconLabel->setPixmap({});
    }
}

} // namespace nx::vms::client::desktop

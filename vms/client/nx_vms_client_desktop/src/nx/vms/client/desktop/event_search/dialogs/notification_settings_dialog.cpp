// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_settings_dialog.h"

#include <nx/vms/client/desktop/style/helper.h>

#include "private/popup_settings_widget.h"

namespace nx::vms::client::desktop {

NotificationSettingsDialog::NotificationSettingsDialog(
    SiteNotificationSettingsManager* siteNotificationSettingsManager,
    QWidget* parent)
    :
    base_type{parent},
    m_settingsWidget{new PopupSettingsWidget{siteNotificationSettingsManager, this}}
{
    resize(400, 600);
    setMinimumSize(QSize(400, 600));

    setWindowTitle(tr("Select Notification Types"));

    setModal(true);

    auto layout = new QVBoxLayout{this};
    layout->setSpacing(0);
    layout->setContentsMargins(style::Metrics::kDefaultChildMargins);

    layout->addWidget(m_settingsWidget);

    auto buttonBoxLine = new QFrame;
    buttonBoxLine->setFrameShape(QFrame::HLine);
    buttonBoxLine->setFrameShadow(QFrame::Sunken);
    layout->addWidget(buttonBoxLine);

    auto buttonBox = new QDialogButtonBox{this};
    buttonBox->setOrientation(Qt::Horizontal);
    buttonBox->setStandardButtons(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    layout->addWidget(buttonBox);
}

void NotificationSettingsDialog::accept()
{
    m_settingsWidget->applyChanges();

    base_type::accept();
}

void NotificationSettingsDialog::showEvent(QShowEvent* event)
{
    m_settingsWidget->loadDataToUi();

    base_type::showEvent(event);
}

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_settings_dialog.h"
#include "ui_videowall_settings_dialog.h"

#include <core/resource/videowall_resource.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <platform/platform_abstraction.h>

using nx::vms::client::desktop::VideoWallShortcutHelper;

QnVideowallSettingsDialog::QnVideowallSettingsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::QnVideowallSettingsDialog)
{
    ui->setupUi(this);
    setResizeToContentsMode(Qt::Vertical);
}

QnVideowallSettingsDialog::~QnVideowallSettingsDialog() {}

void QnVideowallSettingsDialog::loadFromResource(const QnVideoWallResourcePtr& videowall) {
    ui->autoRunCheckBox->setChecked(videowall->isAutorun());
    ui->enableTimelineCheckBox->setChecked(videowall->isTimelineEnabled());

    bool shortcutsSupported = qnPlatform->shortcuts()->supported();
    ui->shortcutCheckbox->setVisible(shortcutsSupported);

    if (!shortcutsSupported)
        return;

    VideoWallShortcutHelper videoWallShortcutHelper;
    bool exists = videoWallShortcutHelper.shortcutExists(videowall);
    ui->shortcutCheckbox->setChecked(exists);
    if (!exists && !videoWallShortcutHelper.canStartVideoWall(videowall))
        ui->shortcutCheckbox->setEnabled(false);
}

void QnVideowallSettingsDialog::submitToResource(const QnVideoWallResourcePtr& videowall) {
    videowall->setAutorun(ui->autoRunCheckBox->isChecked());
    videowall->setTimelineEnabled(ui->enableTimelineCheckBox->isChecked());

    if (!qnPlatform->shortcuts()->supported())
        return;

    VideoWallShortcutHelper videoWallShortcutHelper;
    if (!ui->shortcutCheckbox->isChecked())
        videoWallShortcutHelper.deleteShortcut(videowall);
    else if (videoWallShortcutHelper.canStartVideoWall(videowall))
        videoWallShortcutHelper.createShortcut(videowall);
}

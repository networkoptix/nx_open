// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_widget.h"
#include "ui_logs_management_widget.h"

#include <nx/vms/client/desktop/style/skin.h>

namespace nx::vms::client::desktop {

LogsManagementWidget::LogsManagementWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::LogsManagementWidget)
{
    setupUi();
}

LogsManagementWidget::~LogsManagementWidget()
{
}

void LogsManagementWidget::loadDataToUi()
{
}

void LogsManagementWidget::applyChanges()
{
}

bool LogsManagementWidget::hasChanges() const
{
    return false;
}

void LogsManagementWidget::setReadOnlyInternal(bool readOnly)
{
}

void LogsManagementWidget::setupUi()
{
    ui->setupUi(this);

    ui->downloadButton->setIcon(qnSkin->icon("text_buttons/download.png"));
    ui->settingsButton->setIcon(qnSkin->icon("text_buttons/settings.png"));
    ui->resetButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    ui->openFolderButton->setIcon(qnSkin->icon("tree/local.png")); // FIXME

    ui->resetButton->setFlat(true);
    ui->openFolderButton->setFlat(true);

    ui->downloadButton->setEnabled(false);
    ui->settingsButton->setEnabled(false);
}

} // namespace nx::vms::client::desktop

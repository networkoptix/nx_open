// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_rebuild_widget.h"
#include "ui_storage_rebuild_widget.h"

#include <nx/vms/api/data/storage_scan_info.h>

QnStorageRebuildWidget::QnStorageRebuildWidget(QWidget* parent) :
    base_type(parent),
    ui(new Ui::StorageRebuildWidget())
{
    ui->setupUi(this);
    loadData(nx::vms::api::StorageScanInfo(), false);

    connect(ui->stopButton, &QPushButton::clicked, this, [this]
    {
        ui->stopButton->setEnabled(false);
        emit cancelRequested();
    });
}

QnStorageRebuildWidget::~QnStorageRebuildWidget()
{
}

void QnStorageRebuildWidget::loadData(const nx::vms::api::StorageScanInfo& data, bool isBackup)
{
    QString placeholder = lit(" \t%p%");
    if (data.progress >= 0)
    {
        ui->progressBar->setValue(data.totalProgress * 100 + 0.5);
        switch (data.state)
        {
            case nx::vms::api::RebuildState::partial:
                ui->progressBar->setFormat(isBackup
                    ? tr("Fast Backup Scan...") + placeholder
                    : tr("Fast Archive Scan...") + placeholder);
                break;
            case nx::vms::api::RebuildState::full:
                ui->progressBar->setFormat(isBackup
                    ? tr("Reindexing Backup...") + placeholder
                    : tr("Reindexing Archive...") + placeholder);
                break;
            default:
                break;
        }
    }

    ui->stopButton->setEnabled(data.state == nx::vms::api::RebuildState::full);
    setVisible(data.state == nx::vms::api::RebuildState::partial || data.state == nx::vms::api::RebuildState::full);
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace Ui {
    class StorageRebuildWidget;
} // namespace Ui

namespace nx::vms::api { struct StorageScanInfo; }

class QnStorageRebuildWidget : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    QnStorageRebuildWidget(QWidget* parent = nullptr);
    virtual ~QnStorageRebuildWidget();

    void loadData(const nx::vms::api::StorageScanInfo &data, bool isBackup);

signals:
    void cancelRequested();

private:
    QScopedPointer<Ui::StorageRebuildWidget> ui;
};

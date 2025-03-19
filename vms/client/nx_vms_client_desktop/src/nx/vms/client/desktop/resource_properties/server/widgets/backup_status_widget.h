// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace Ui { class BackupStatusWidget; }

namespace nx::vms::client::desktop {

class SystemContext;

class BackupStatusWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    BackupStatusWidget(QWidget* parent = nullptr);
    virtual ~BackupStatusWidget() override;

    void setServer(const QnMediaServerResourcePtr& server);
    Q_INVOKABLE void updateBackupStatus();

public:
    virtual void showEvent(QShowEvent* event) override;

private:
    void setupSkipBackupButton();
    Q_INVOKABLE void onBackupTimePointCalculated(
        const QnMediaServerResourcePtr server,
        const std::chrono::milliseconds timePoint);

private:
    const std::unique_ptr<Ui::BackupStatusWidget> ui;
    QnMediaServerResourcePtr m_server;
    bool m_hasBackupStorageIssue = false;
    std::atomic<bool> m_interruptBackupQueueSizeCalculation;
};

} // namespace nx::vms::client::desktop

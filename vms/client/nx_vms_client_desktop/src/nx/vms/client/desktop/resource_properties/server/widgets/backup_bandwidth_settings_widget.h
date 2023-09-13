// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace Ui { class BackupBandwidthSettingsWidget; }

namespace nx::vms::client::desktop {

class BackupBandwidthScheduleCellPainter;

class BackupBandwidthSettingsWidget:
    public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    BackupBandwidthSettingsWidget(QWidget* parent = nullptr);
    virtual ~BackupBandwidthSettingsWidget() override;

    void setServer(const QnMediaServerResourcePtr& server);

    bool hasChanges() const;
    void loadDataToUi();
    void applyChanges();
    void discardChanges();

signals:
    void hasChangesChanged();

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void updateBannerText();
    void resetScheduleBrushButton();

private:
    const std::unique_ptr<Ui::BackupBandwidthSettingsWidget> ui;
    const std::unique_ptr<BackupBandwidthScheduleCellPainter> m_scheduleCellPainter;
    QnMediaServerResourcePtr m_server;
    bool m_showSheduledLimitEditHint = true;
};

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>

namespace Ui { class InfoOverlaySettingsWidget; }

namespace nx::vms::client::desktop {

class InfoOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    InfoOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~InfoOverlaySettingsWidget() override;

    const ExportInfoOverlayPersistentSettings& data() const;
    void setData(const ExportInfoOverlayPersistentSettings& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

    int maxOverlayHeight() const;
    void setMaxOverlayHeight(int value);

signals:
    void dataChanged(const ExportInfoOverlayPersistentSettings& data);
    void deleteClicked();

private:
    void updateControls();

private:
    QScopedPointer<Ui::InfoOverlaySettingsWidget> ui;
    ExportInfoOverlayPersistentSettings m_data;
};

} // namespace nx::vms::client::desktop

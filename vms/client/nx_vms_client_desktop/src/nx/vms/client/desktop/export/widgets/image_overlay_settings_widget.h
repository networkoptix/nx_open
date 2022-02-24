// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>

namespace Ui { class ImageOverlaySettingsWidget; }

namespace nx::vms::client::desktop {

class ImageOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ImageOverlaySettingsWidget(QWidget* parent = nullptr);
    virtual ~ImageOverlaySettingsWidget() override;

    const ExportImageOverlayPersistentSettings& data() const;
    void setData(const ExportImageOverlayPersistentSettings& data);

    int maxOverlayWidth() const;
    void setMaxOverlayWidth(int value);

signals:
    void dataChanged(const ExportImageOverlayPersistentSettings& data);
    void deleteClicked();

private:
    void updateControls();
    void browseForFile();

private:
    QScopedPointer<Ui::ImageOverlaySettingsWidget> ui;
    ExportImageOverlayPersistentSettings m_data;
    QString m_lastImageDir;
};

} // namespace nx::vms::client::desktop

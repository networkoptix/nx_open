// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QLayout>
#include <QtWidgets/QWidget>

#include <nx/vms/api/data/pixelation_settings.h>

namespace Ui { class ExportMetadataSettingsWidget; }

namespace nx::vms::client::desktop {

class ExportMetadataSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    struct Data
    {
        bool motionChecked = false;
        bool objectsChecked = false;
        bool showAttributes = false;

        api::ObjectTypeSettings objectTypeSettings;
    };

    ExportMetadataSettingsWidget(QWidget* parent = nullptr);
    virtual ~ExportMetadataSettingsWidget();

    void setData(const Data& data);
    void setDeviceId(const nx::Uuid& deviceId);

signals:
    void dataEdited(Data data);
    void deleteClicked();

private:
    void openObjectTypeSelectionDialog();
    Data buildDataFromUi() const;

    QScopedPointer<Ui::ExportMetadataSettingsWidget> ui;
    api::ObjectTypeSettings m_objectTypeSettings;
    std::optional<nx::Uuid> m_deviceId = std::nullopt;

    void emitDataEdited();
};

} // namespace nx::vms::client::desktop

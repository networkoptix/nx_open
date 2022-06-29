// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>


namespace Ui { class CustomSettingsWidget; }

struct QnCameraAdvancedParams;
struct QnCameraAdvancedParamValue;
using QnCameraAdvancedParamValueList = QList<QnCameraAdvancedParamValue>;

namespace nx::vms::client::desktop {

class CustomSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;
public:
    CustomSettingsWidget(QWidget* parent = nullptr);
    virtual ~CustomSettingsWidget() override;

    void loadManifest(const QnCameraAdvancedParams& manifest);
    void loadValues(const QnCameraAdvancedParamValueList& params);

private:
    struct Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::CustomSettingsWidget> ui;
};

} // namespace nx::vms::client::desktop

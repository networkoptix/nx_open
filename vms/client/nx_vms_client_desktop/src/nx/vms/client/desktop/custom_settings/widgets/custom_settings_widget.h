#pragma once

#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

namespace Ui { class CustomSettingsWidget; }

struct QnCameraAdvancedParams;
struct QnCameraAdvancedParamValue;
using QnCameraAdvancedParamValueList = QList<QnCameraAdvancedParamValue>;

namespace nx::vms::client::desktop {

class CustomSettingsWidget: public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;
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

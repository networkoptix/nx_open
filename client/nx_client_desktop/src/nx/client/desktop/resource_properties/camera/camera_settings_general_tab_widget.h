#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <QtCore/QPointer>

namespace Ui {
class CameraSettingsGeneralTabWidget;
}

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsModel;

class CameraSettingsGeneralTabWidget: public QnAbstractPreferencesWidget
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit CameraSettingsGeneralTabWidget(CameraSettingsModel* model,
        QWidget* parent = nullptr);
    virtual ~CameraSettingsGeneralTabWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    QScopedPointer<Ui::CameraSettingsGeneralTabWidget> ui;
    CameraSettingsModel* const m_model;
};

} // namespace desktop
} // namespace client
} // namespace nx

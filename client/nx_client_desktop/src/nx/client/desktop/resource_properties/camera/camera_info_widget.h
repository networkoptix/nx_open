#pragma once

#include <ui/widgets/common/panel.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui { class CameraInfoWidget; }

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsModel;

class CameraInfoWidget: public QnPanel, public QnAbstractPreferencesInterface
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    explicit CameraInfoWidget(QWidget* parent = nullptr);
    virtual ~CameraInfoWidget() override;

    void setModel(CameraSettingsModel* model);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    void alignLabels();
    void updatePalette();
    void updatePageSwitcher();
    void updateNetworkInfo();

private:
    QScopedPointer<Ui::CameraInfoWidget> ui;
    CameraSettingsModel* m_model = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx

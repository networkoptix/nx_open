#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/customization/customized.h>

class QButtonGroup;
class QQuickView;
class QQuickItem;
class QShowEvent;
class QHideEvent;

namespace Ui { class CameraMotionSettingsWidget; }

namespace nx::vms::client::core { class CameraMotionHelper; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraMotionSettingsWidget: public Customized<QWidget>
{
    Q_OBJECT
    using base_type = Customized<QWidget>;

    Q_PROPERTY(QVector<QColor> sensitivityColors READ sensitivityColors WRITE setSensitivityColors)

public:
    explicit CameraMotionSettingsWidget(
        CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~CameraMotionSettingsWidget() override;

    QVector<QColor> sensitivityColors() const;
    void setSensitivityColors(const QVector<QColor>& value);

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    QQuickItem* motionItem() const;
    void loadState(const CameraSettingsDialogState& state);
    void loadAlerts(const CameraSettingsDialogState& state);
    void resetMotionRegions();

private:
    const QScopedPointer<Ui::CameraMotionSettingsWidget> ui;
    const QScopedPointer<core::CameraMotionHelper> m_motionHelper;
    QButtonGroup* const m_sensitivityButtons = nullptr;
    QQuickView* const m_motionView = nullptr;
    QVector<QColor> m_sensitivityColors;
    QString m_cameraId;
};

} // namespace nx::vms::client::desktop

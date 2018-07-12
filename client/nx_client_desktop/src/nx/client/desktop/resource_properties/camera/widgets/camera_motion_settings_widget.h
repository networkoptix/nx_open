#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/customization/customized.h>

class QButtonGroup;
class QQuickView;

namespace Ui { class CameraMotionSettingsWidget; }

namespace nx {
namespace client {

namespace core { class CameraMotionHelper; }

namespace desktop {

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

private:
    void loadState(const CameraSettingsDialogState& state);
    void resetMotionRegions();

private:
    const QScopedPointer<Ui::CameraMotionSettingsWidget> ui;
    const QScopedPointer<core::CameraMotionHelper> m_motionHelper;
    QButtonGroup* const m_sensitivityButtons = nullptr;
    QQuickView* const m_motionView = nullptr;
    QVector<QColor> m_sensitivityColors;
    QString m_cameraId;
};

} // namespace desktop
} // namespace client
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/uuid.h>

class QButtonGroup;
class QQuickWidget;
class QQuickItem;
class QShowEvent;
class QHideEvent;

namespace Ui { class CameraMotionSettingsWidget; }

namespace nx::vms::client::core { class CameraMotionHelper; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraMotionSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraMotionSettingsWidget(
        CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~CameraMotionSettingsWidget() override;

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
    QQuickWidget* const m_motionWidget = nullptr;
    nx::Uuid m_cameraId;
};

} // namespace nx::vms::client::desktop

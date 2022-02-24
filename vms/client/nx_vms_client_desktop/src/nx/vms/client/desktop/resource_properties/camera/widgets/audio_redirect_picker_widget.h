// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <memory>

#include <core/resource/resource_fwd.h>

namespace Ui { class AudioRedirectPickerWidget; }

class QMenu;

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;

class AudioRedirectPickerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    AudioRedirectPickerWidget(QWidget* parent = nullptr);
    virtual ~AudioRedirectPickerWidget() override;

    enum StreamDirection
    {
        Input,
        Output,
    };
    void setup(StreamDirection streamDirection);

    void loadState(const CameraSettingsDialogState& state);

signals:
    void audioRedirectDeviceIdChanged(const QnUuid& deviceId);

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    QnResourcePool* resourcePool() const;

    void selectAudioRedirectDevice();

    bool capabilityCheck(const QnResourcePtr& resource) const;
    bool sameServerCheck(const QnResourcePtr& resource) const;

    QnUuid getRedirectDeviceId(const CameraSettingsDialogState& state) const;

    QString activeRedirectLabelText() const;
    QString inactiveRedirectLabelText() const;
    QString requiredRedirectLabelText() const;

    QString activeRedirectMenuItemText() const;
    QString inactiveRedirectMenuItemText() const;

    QString invalidRedirectErrorText() const;
    QString differentServersErrorText() const;

private:
    const std::unique_ptr<Ui::AudioRedirectPickerWidget> ui;
    std::optional<StreamDirection> m_streamDirection;

    std::unique_ptr<QMenu> m_activeRedirectDropdownMenu;
    QAction* m_activeRedirectDropdownAction = nullptr;

    std::unique_ptr<QMenu> m_inactiveRedirectDropdownMenu;
    QAction* m_inactiveRedirectDropdownAction = nullptr;

    QnUuid m_deviceId;
    QnUuid m_audioRedirectDeviceId;
};

} // namespace nx::vms::client::desktop

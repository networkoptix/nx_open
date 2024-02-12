// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace Ui { class CameraAdvancedParamsWidget; }

namespace nx::vms::client::core::ptz {class RemotePtzController; }

namespace nx::vms::client::desktop {

class CameraAdvancedParamWidgetsManager;
class CameraSettingsDialogStore;
struct CameraSettingsDialogState;

class CameraAdvancedParamsWidget: public QWidget,
    public core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraAdvancedParamsWidget(CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~CameraAdvancedParamsWidget();

    void loadState(const CameraSettingsDialogState& state);

    void setSelectedServer(const nx::Uuid& serverId);
    void setPtzInterface(std::unique_ptr<core::ptz::RemotePtzController> controller);

    void saveValues();

    bool hasChanges() const;

signals:
    void hasChangesChanged();

private:
    void refreshValues();

    void saveSingleValue(const QnCameraAdvancedParameter& parameter, const QString& value);
    void sendCustomParameterCommand(
        const QnCameraAdvancedParameter& parameter, const QString& value);

    void updateButtonsState();
    void updateParametersVisibility();

    // Returns current values of all parameters that belong to the given group set.
    QnCameraAdvancedParamValueMap groupParameters(const QSet<QString>& groups) const;

    /** Sends setCameraParam request to mediaserver. */
    bool sendSetCameraParams(const QnCameraAdvancedParamValueList& values);
    /** Sends getCameraParam request to mediaserver. */
    bool sendGetCameraParams(const QStringList& keys);

private:
    void at_advancedParamChanged(const QString &id, const QString &value);
    void at_advancedSettingsLoaded(bool success, int handle, const QnCameraAdvancedParamValueList &params);
    void at_advancedParam_saved(bool success, int handle, const QnCameraAdvancedParamValueList &params);

private:
    enum class State
    {
        Init,
        Loading,
        Applying
    };
    State state() const;
    void setState(State newState);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    nx::utils::ImplPtr<Ui::CameraAdvancedParamsWidget> ui;

    QScopedPointer<CameraAdvancedParamWidgetsManager> m_advancedParamWidgetsManager;

    int m_paramRequestHandle = 0;
    State m_state = State::Init;
    QnCameraAdvancedParamValueMap m_loadedValues;
    QnCameraAdvancedParamValueMap m_currentValues;
};

} // namespace nx::vms::client::desktop

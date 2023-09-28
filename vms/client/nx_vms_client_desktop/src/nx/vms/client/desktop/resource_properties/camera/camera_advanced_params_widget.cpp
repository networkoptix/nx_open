// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_advanced_params_widget.h"
#include "ui_camera_advanced_params_widget.h"

#include <api/server_rest_connection.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/ptz/remote_ptz_controller.h>
#include <ui/common/read_only.h>

#include "camera_advanced_param_widgets_manager.h"
#include "flux/camera_settings_dialog_state.h"
#include "flux/camera_settings_dialog_store.h"
#include "widgets/private/motion_stream_alerts.h"

namespace {

QSet<QString> parameterIds(const QnCameraAdvancedParamGroup& group)
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup& subGroup: group.groups)
        result.unite(parameterIds(subGroup));
    for (const QnCameraAdvancedParameter& param: group.params)
    {
        if (param.hasValue())
            result.insert(param.id);
    }
    return result;
}

QSet<QString> parameterIds(const QnCameraAdvancedParams& params)
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup& group: params.groups)
        result.unite(parameterIds(group));
    return result;
}

QnCameraAdvancedParamValueMap differenceMap(
    const QnCameraAdvancedParamValueMap& left,
    const QnCameraAdvancedParamValueMap& right)
{
    QnCameraAdvancedParamValueMap result;
    for (auto iter = left.cbegin(); iter != left.cend(); ++iter)
    {
        if (right.contains(iter.key()) && right[iter.key()] == iter.value())
            continue;

        result[iter.key()] = iter.value();
    }
    return result;
}

bool differsFrom(
    const QnCameraAdvancedParamValueMap& left,
    const QnCameraAdvancedParamValueMap& right)
{
    for (auto it = left.cbegin(); it != left.cend(); ++it)
    {
        if (!right.contains(it.key()) || right.value(it.key()) != it.value())
            return true;
    }
    return false;
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraAdvancedParamsWidget::Private
{
    QPointer<CameraSettingsDialogStore> store;
    QnUuid selectedServer;
    std::unique_ptr<core::ptz::RemotePtzController> ptzController;
    QnCameraAdvancedParams manifest;
    QnUuid cameraId;

    bool isValid = false;
    bool isCurrentTab = false; //< Values must be requested only when Advanced tab is opened.
    bool cameraIsOnline = false;
};

CameraAdvancedParamsWidget::CameraAdvancedParamsWidget(
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private()),
    ui(new Ui::CameraAdvancedParamsWidget)
{
    ui->setupUi(this);

    ui->motionImplicitlyDisabledAlertBar->init({.level = BarDescription::BarLevel::Error});
    const auto forceDetectionButton = new QPushButton(ui->motionImplicitlyDisabledAlertBar);
    forceDetectionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    forceDetectionButton->setText(tr("Force Motion Detection"));
    ui->motionImplicitlyDisabledAlertBar->addButton(forceDetectionButton);

    connect(forceDetectionButton, &QPushButton::clicked, store,
        [store]() { store->forceMotionDetection(); });

    QList<int> sizes = ui->splitter->sizes();
    if (NX_ASSERT(sizes.size() == 2))
    {
        sizes[0] = 200;
        sizes[1] = 400;
        ui->splitter->setSizes(sizes);
    }

    m_advancedParamWidgetsManager.reset(
        new CameraAdvancedParamWidgetsManager(ui->groupsWidget, ui->contentsWidget));
    connect(m_advancedParamWidgetsManager.get(),
        &CameraAdvancedParamWidgetsManager::paramValueChanged,
        this,
        &CameraAdvancedParamsWidget::at_advancedParamChanged);

    connect(ui->loadButton, &QPushButton::clicked, this,
        &CameraAdvancedParamsWidget::refreshValues);

    NX_ASSERT(store);
    d->store = store;
    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraAdvancedParamsWidget::loadState);
}

CameraAdvancedParamsWidget::~CameraAdvancedParamsWidget()
{
}

void CameraAdvancedParamsWidget::loadState(const CameraSettingsDialogState& state)
{
    d->isValid = state.isSingleCamera()
        && state.singleCameraProperties.advancedSettingsManifest.has_value();

    d->cameraId = state.singleCameraProperties.id;

    const bool isCurrentTab = state.selectedTab == CameraSettingsTab::advanced;
    const bool switchedToCurrentTab = (isCurrentTab && !d->isCurrentTab);
    d->isCurrentTab = isCurrentTab;

    if (d->isValid)
    {
        const auto manifest = *state.singleCameraProperties.advancedSettingsManifest;
        const bool cameraIsOnline = state.singleCameraProperties.isOnline;

        const bool needReinitialization = d->cameraIsOnline != cameraIsOnline
            || d->manifest != manifest;

        d->cameraIsOnline = cameraIsOnline;
        d->manifest = manifest;

        if (needReinitialization)
        {
            m_paramRequestHandle = 0;
            m_currentValues.clear();
            m_loadedValues.clear();
            setState(State::Init);
            m_advancedParamWidgetsManager->resetManifest(d->manifest);
            updateParametersVisibility();
            if (state.selectedTab == CameraSettingsTab::advanced)
                refreshValues();
        }
        else if (switchedToCurrentTab)
        {
            refreshValues();
        }
    }
    else
    {
        d->manifest = {};
        d->cameraIsOnline = false;
        m_paramRequestHandle = 0;
        m_currentValues.clear();
        m_loadedValues.clear();
        setState(State::Init);
        m_advancedParamWidgetsManager->clear();
    }

    ui->motionImplicitlyDisabledAlertBar->setText(MotionStreamAlerts::implicitlyDisabledAlert(
        state.motion.streamAlert,
        !state.isSingleCamera()));

    this->setEnabled(d->isValid);
    if (d->isValid)
        ::setReadOnly(this, state.readOnly);

    updateButtonsState();
}

void CameraAdvancedParamsWidget::setSelectedServer(const QnUuid& serverId)
{
    d->selectedServer = serverId;
}

void CameraAdvancedParamsWidget::setPtzInterface(
    std::unique_ptr<core::ptz::RemotePtzController> controller)
{
    d->ptzController = std::move(controller);
}

void CameraAdvancedParamsWidget::refreshValues()
{
    // Check that we are in the correct state.
    if (state() != State::Init || !d->cameraIsOnline)
        return;

    const QStringList paramIds = parameterIds(d->manifest).values();
    if (paramIds.isEmpty())
        return;

    NX_VERBOSE(this, "Requesting the following parameters: %1", paramIds);

    // Update state.
    if (sendGetCameraParams(paramIds))
        setState(State::Loading);
}

void CameraAdvancedParamsWidget::saveSingleValue(
    const QnCameraAdvancedParameter& parameter,
    const QString& value)
{
    // Check that we are in the correct state.
    if (state() != State::Init || (!d->cameraIsOnline && !parameter.availableInOffline))
        return;

    // Update state.
    NX_VERBOSE(this, "Update single value %1 to %2", parameter.id, value);
    if (sendSetCameraParams({QnCameraAdvancedParamValue(parameter.id, value)}))
        setState(State::Applying);
}

void CameraAdvancedParamsWidget::sendCustomParameterCommand(
    const QnCameraAdvancedParameter& parameter,
    const QString& value)
{
    using namespace nx::vms::common::ptz;

    // Check that we are in the correct state.
    if (state() != State::Init || (!d->cameraIsOnline && !parameter.availableInOffline))
        return;

    if (!d->ptzController)
        return;

    bool ok = false;

    Options options{Type::configurational};

    if(parameter.writeCmd == lit("custom_zoom"))
    {
        // Expecting a single value.
        Vector speed;
        qreal val = value.toDouble(&ok);
        if (ok)
        {
            speed.zoom = val * 0.01;
            qDebug() << "Sending custom_zoom(" << val << ")";
            d->ptzController->continuousMove(speed, options);
        }
    }
    else if (parameter.writeCmd == lit("custom_ptr"))
    {
        // Expecting a value like "horisontal,vertical,rotation".
        QStringList values = value.split(',');

        if (values.size() != 3)
            return;

        Vector speed;

        // Workaround for VMS-15380: changing sign of 'pan' control to make camera
        // rotate in a proper direction.
        // TODO: We need some flag to set a proper direction for camera rotation.
        speed.pan = -values[0].toDouble(&ok);
        speed.tilt = values[1].toDouble(&ok);

        // Control provides the angle in range [-180; 180],
        // but we should send values in range [-1.0, 1.0].
        speed.rotation = qBound(values[2].toDouble(&ok) / 180.0, -1.0, 1.0);

        qDebug() << "Sending custom_ptr(pan=" << speed.pan << ", tilt=" << speed.tilt << ", rot=" << speed.rotation << ")";

        d->ptzController->continuousMove(speed, options);
    }
    else if (parameter.writeCmd == lit("custom_focus"))
    {
        // Expecting a single value.
        qreal speed = value.toDouble(&ok);
        if (ok)
        {
            speed *= 0.01;
            qDebug() << "Sending custom_focus(" << speed << ")";
            d->ptzController->continuousFocus(speed, options);
        }
    }
}

void CameraAdvancedParamsWidget::saveValues()
{
    // Check that we are in the correct state.
    if (state() != State::Init)
        return;

    auto modifiedValues = differenceMap(m_currentValues, m_loadedValues);
    if (!d->cameraIsOnline)
    {
        // Remove parameters not available in offline.
        for (auto it = modifiedValues.begin(); it != modifiedValues.end(); /*no increment*/)
        {
            const auto parameter = d->manifest.getParameterById(it.key());
            if (!parameter.availableInOffline)
                it = modifiedValues.erase(it);
            else
                ++it;
        }
    }

    if (modifiedValues.isEmpty())
        return;

    QSet<QString> groups;
    for (auto it = modifiedValues.cbegin(); it != modifiedValues.cend(); ++it)
    {
        const auto parameter = d->manifest.getParameterById(it.key());
        if (!parameter.group.isEmpty())
            groups.insert(parameter.group);
    }

    const auto additionalValues = groupParameters(groups);
    for (auto it = additionalValues.cbegin(); it != additionalValues.cend(); ++it)
    {
        if (!modifiedValues.contains(it.key()))
            modifiedValues[it.key()] = it.value();
    }

    NX_VERBOSE(this, "Applying %1 values", modifiedValues.size());
    // Update state.
    if (sendSetCameraParams(QnCameraAdvancedParamValueMapHelper(modifiedValues).toValueList()))
        setState(State::Applying);
}

bool CameraAdvancedParamsWidget::hasChanges() const
{
    return differsFrom(m_currentValues, m_loadedValues);
}

void CameraAdvancedParamsWidget::updateButtonsState()
{
    ui->loadButton->setEnabled(d->isValid && d->cameraIsOnline && m_state == State::Init);
}

void CameraAdvancedParamsWidget::updateParametersVisibility()
{
    m_advancedParamWidgetsManager->updateParametersVisibility(d->cameraIsOnline
        ? CameraAdvancedParamWidgetsManager::ParameterVisibility::showAll
        : CameraAdvancedParamWidgetsManager::ParameterVisibility::showOfflineOnly);
}

QnCameraAdvancedParamValueMap CameraAdvancedParamsWidget::groupParameters(
    const QSet<QString>& groups) const
{
    QnCameraAdvancedParamValueMap result;
    const auto ids = d->manifest.allParameterIds();

    for (const auto& id: ids)
    {
        const auto parameter = d->manifest.getParameterById(id);
        if (groups.contains(parameter.group))
        {
            const auto parameterValue = m_advancedParamWidgetsManager->parameterValue(parameter.id);
            if (parameterValue != std::nullopt)
                result[parameter.id] = *parameterValue;
        }
    }

    return result;
}

bool CameraAdvancedParamsWidget::sendSetCameraParams(const QnCameraAdvancedParamValueList& values)
{
    if (d->selectedServer.isNull() || !connection())
        return false;

    QnCameraAdvancedParamsPostBody body;
    body.cameraId = d->cameraId.toString();
    for (const auto& value: values)
        body.paramValues.insert(value.id, value.value);

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, nx::network::rest::JsonResult data)
        {
            NX_VERBOSE(this, "Parameter is applied, success: %1", success);
            auto response = data.deserialized<QnCameraAdvancedParamValueList>();
            at_advancedParam_saved(success, handle, response);
        });

    m_paramRequestHandle = connectedServerApi()->postJsonResult(
        "/api/setCameraParam",
        /*params*/ {},
        QJson::serialized(body),
        callback,
        thread(),
        /*timeouts*/ {},
        d->selectedServer);
    return m_paramRequestHandle != 0;
}

bool CameraAdvancedParamsWidget::sendGetCameraParams(const QStringList& keys)
{
    if (d->selectedServer.isNull() || !connection())
        return false;

    nx::network::rest::Params params;
    params.insert("cameraId", d->cameraId);
    for (const QString& param: keys)
        params.insert(param, QString());

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, nx::network::rest::JsonResult data)
        {
            NX_VERBOSE(this, "Values are received, success: %1", success);
            auto response = data.deserialized<QnCameraAdvancedParamValueList>();
            at_advancedSettingsLoaded(success, handle, response);
        });
    m_paramRequestHandle = connectedServerApi()->getJsonResult(
        "/api/getCameraParam",
        params,
        callback,
        thread(),
        d->selectedServer);
    return m_paramRequestHandle != 0;
}

void CameraAdvancedParamsWidget::at_advancedParamChanged(const QString& id, const QString& value)
{
    // Check that we are in correct state.
    if (state() != State::Init)
        return;

    // Check parameter validity.
    const auto parameter = d->manifest.getParameterById(id);
    if (!parameter.isValid())
        return;

    if (parameter.isCustomControl())
    {
        // This is a custom parameter with specific API.
        sendCustomParameterCommand(parameter, value);
    }
    else if (parameter.isInstant())
    {
        // Apply instant parameters immediately.
        saveSingleValue(parameter, value);
    }
    else
    {
        NX_ASSERT(parameter.hasValue());

        // Queue modified parameter.
        m_currentValues[id] = value;
        emit hasChangesChanged();
    }
}

void CameraAdvancedParamsWidget::at_advancedSettingsLoaded(
    bool success, int handle, const QnCameraAdvancedParamValueList& params)
{
    /* Check that we are in the correct state. */
    if (state() != State::Loading)
        return;

    /* Check that we are waiting for this request. */
    if (handle != m_paramRequestHandle)
        return;

    /* Show error if something was not correct. */
    if (!success)
    {
        NX_WARNING(this, "Request failed. Camera %1", d->cameraId);
        setState(State::Init);
        return;
    }

    /* Save loaded values in cache. */
    m_loadedValues = QnCameraAdvancedParamValueMapHelper(m_loadedValues).appendValueList(params);

    /* Update modified parameters list. */
    for (const QnCameraAdvancedParamValue &value: params)
        m_currentValues.remove(value.id);

    /* Display values. */
    m_advancedParamWidgetsManager->loadValues(params, /*packetMode*/ true);

    /* Update state. */
    setState(State::Init);

    emit hasChangesChanged();
}

void CameraAdvancedParamsWidget::at_advancedParam_saved(
    bool success, int handle, const QnCameraAdvancedParamValueList &params)
{
    /* Check that we are in the correct state. */
    if (state() != State::Applying)
        return;

    /* Check that we are waiting for this request. */
    if (handle != m_paramRequestHandle)
        return;

    /* Show error if something was not correct. */
    if (!success)
    {
        // TODO: #sivanov Handle errors.
        setState(State::Init);
        refreshValues();
        return;
    }

    /* Update stored parameters. */
    bool needResync = false;
    for (const QnCameraAdvancedParamValue& value: params)
    {
        m_loadedValues[value.id] = value.value;
        m_currentValues.remove(value.id);

        if (d->manifest.getParameterById(value.id).resync)
            needResync = true;
    }

    // Update values with new ones in the case of packet mode.
    if (d->manifest.packet_mode)
        m_advancedParamWidgetsManager->loadValues(params, /*packetMode*/ true);

    /* Update state. */
    setState(State::Init);
    emit hasChangesChanged();

    // Reload all values if one of the parameters requires resync.
    if (needResync && !d->manifest.packet_mode)
        refreshValues();
}

CameraAdvancedParamsWidget::State CameraAdvancedParamsWidget::state() const
{
    return m_state;
}

void CameraAdvancedParamsWidget::setState(State newState)
{
    if (newState == m_state)
        return;

    NX_VERBOSE(this, "Set state %1", (int)newState);
    m_state = newState;

    auto textByState =
        [](State state)
        {
            switch (state)
            {
                case State::Init:
                    return QString();
                case State::Loading:
                    return tr("Loading values...");
                case CameraAdvancedParamsWidget::State::Applying:
                    return tr("Applying changes...");
            }
            return QString();
        };

    ui->progressWidget->setText(textByState(m_state));
    ui->progressWidget->setVisible(m_state != State::Init);
    updateButtonsState();
}

} // namespace nx::vms::client::desktop

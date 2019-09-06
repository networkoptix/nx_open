#include "camera_advanced_params_widget.h"
#include "ui_camera_advanced_params_widget.h"
#include "camera_advanced_param_widgets_manager.h"

#include <api/server_rest_connection.h>

#include <core/ptz/remote_ptz_controller.h>
#include <core/resource/param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/xml/camera_advanced_param_reader.h>
#include <nx/utils/guarded_callback.h>

namespace {

bool parameterHasValue(const QnCameraAdvancedParameter &param)
{
    return param.isValid() && QnCameraAdvancedParameter::dataTypeHasValue(param.dataType);
}

bool parameterIsInstant(const QnCameraAdvancedParameter &param)
{
    return param.isValid() && !QnCameraAdvancedParameter::dataTypeHasValue(param.dataType);
}

QSet<QString> parameterIds(const QnCameraAdvancedParamGroup& group)
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &subGroup : group.groups)
        result.unite(parameterIds(subGroup));
    for (const QnCameraAdvancedParameter &param : group.params)
        if (parameterHasValue(param))
            result.insert(param.id);
    return result;
}

QSet<QString> parameterIds(const QnCameraAdvancedParams& params)
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &group : params.groups)
        result.unite(parameterIds(group));
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

CameraAdvancedParamsWidget::CameraAdvancedParamsWidget(QWidget* parent /*= NULL*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedParamsWidget),
    m_advancedParamsReader(new QnCachingCameraAdvancedParamsReader()),
    m_cameraAvailable(false),
    m_paramRequestHandle(0),
    m_state(State::Init)
{
    ui->setupUi(this);

    m_advancedParamWidgetsManager.reset(
        new CameraAdvancedParamWidgetsManager(ui->groupsWidget, ui->contentsWidget));
    connect(m_advancedParamWidgetsManager, &CameraAdvancedParamWidgetsManager::paramValueChanged,
        this, &CameraAdvancedParamsWidget::at_advancedParamChanged);

    connect(ui->loadButton, &QPushButton::clicked, this, &CameraAdvancedParamsWidget::loadValues);
}

CameraAdvancedParamsWidget::~CameraAdvancedParamsWidget()
{}

QnVirtualCameraResourcePtr CameraAdvancedParamsWidget::camera() const {
    return m_camera;
}

void CameraAdvancedParamsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;

    if (m_camera)
    {
        connect(m_camera, &QnResource::statusChanged, this,
            &CameraAdvancedParamsWidget::updateCameraAvailability);

        connect(m_camera, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr &resource, const QString &key)
            {
                if (resource != m_camera)
                    return;
                if (key == ResourcePropertyKey::kCameraAdvancedParams)
                    initialize(); //< Re-init state if key parameter changed.
            });
    }

    m_ptzController.reset(new QnRemotePtzController(camera));

    /* Initialize state */
    initialize();
}

void CameraAdvancedParamsWidget::initSplitter() {
    QList<int> sizes = ui->splitter->sizes();
    sizes[0] = 200;
    sizes[1] = 400;
    ui->splitter->setSizes(sizes);
}

void CameraAdvancedParamsWidget::initialize() {

    initSplitter();

    updateCameraAvailability();

    /* Clean state. */
    setState(State::Init);

    if (m_camera)
        m_advancedParamsReader->clearResourceParams(m_camera);

    m_paramRequestHandle = 0;
    m_currentValues.clear();
    m_loadedValues.clear();

    /* Hide progress indicator. */
    ui->progressWidget->setVisible(false);

    /* Reload parameters tree. */
    displayParams();
}

void CameraAdvancedParamsWidget::displayParams() {
    if (!m_camera) {
        m_advancedParamWidgetsManager->clear();
        return;
    }

    auto params = m_advancedParamsReader->params(m_camera);
    m_advancedParamWidgetsManager->displayParams(params);
    updateParameretsVisibility();
}

void CameraAdvancedParamsWidget::loadValues() {
    /* Check that we are in the correct state. */
    if (state() != State::Init || !m_cameraAvailable)
        return;

    /* Check that there are values to request. */
    auto params = m_advancedParamsReader->params(m_camera);
    QStringList paramIds = parameterIds(params).toList();
    if (paramIds.isEmpty())
        return;

    qDebug() << "Requesting the following parameters:" << paramIds;

    /* Update state. */
    if (!sendGetCameraParams(m_camera, paramIds))
        return;
    setState(State::Loading);
}

void CameraAdvancedParamsWidget::saveSingleValue(
    const QnCameraAdvancedParameter& parameter,
    const QString& value)
{
    // Check that we are in the correct state.
    if (state() != State::Init || (!m_cameraAvailable && !parameter.availableInOffline))
        return;

    // Update state.
    if (sendSetCameraParams(m_camera, {QnCameraAdvancedParamValue(parameter.id, value)}))
        setState(State::Applying);
}

void CameraAdvancedParamsWidget::sendCustomParameterCommand(
    const QnCameraAdvancedParameter& parameter,
    const QString& value)
{
    // Check that we are in the correct state.
    if (state() != State::Init || (!m_cameraAvailable && !parameter.availableInOffline))
        return;

    // Check that the server and camera are available.
    const auto serverConnection = getServerConnection(m_camera);
    if (!serverConnection)
        return;
    if (!m_ptzController)
        return;

    bool ok = false;

    nx::core::ptz::Options options{nx::core::ptz::Type::configurational};

    if(parameter.writeCmd == lit("custom_zoom"))
    {
        // Expecting a single value.
        nx::core::ptz::Vector speed;
        qreal val = value.toDouble(&ok);
        if (ok)
        {
            speed.zoom = val * 0.01;
            qDebug() << "Sending custom_zoom(" << val << ")";
            m_ptzController->continuousMove(speed, options);
        }
    }
    else if (parameter.writeCmd == lit("custom_ptr"))
    {
        // Expecting a value like "horisontal,vertical,rotation".
        QStringList values = value.split(L',');

        if (values.size() != 3)
            return;

        nx::core::ptz::Vector speed;

        // Workaround for VMS-15380: changing sign of 'pan' control to make camera
        // rotate in a proper direction.
        // TODO: We need some flag to set a proper direction for camera rotation.
        speed.pan = -values[0].toDouble(&ok);
        speed.tilt = values[1].toDouble(&ok);

        // Control provides the angle in range [-180; 180],
        // but we should send values in range [-1.0, 1.0].
        speed.rotation = qBound(values[2].toDouble(&ok) / 180.0, -1.0, 1.0);

        qDebug() << "Sending custom_ptr(pan=" << speed.pan << ", tilt=" << speed.tilt << ", rot=" << speed.rotation << ")";

        // TODO: We should use PTZ controller instead.
        m_ptzController->continuousMove(speed, options);
    }
    else if (parameter.writeCmd == lit("custom_focus"))
    {
        // Expecting a single value.
        qreal speed = value.toDouble(&ok);
        if (ok)
        {
            speed *= 0.01;
            qDebug() << "Sending custom_focus(" << speed << ")";
            m_ptzController->continuousFocus(speed, options);
        }
    }

    /* Update state. */
    //m_paramRequestHandle = serverConnection->setParamsAsync(m_camera, QnCameraAdvancedParamValueList() << value, this, SLOT(at_advancedParam_saved(int, const QnCameraAdvancedParamValueList &, int)));
    //if (ok)
    //    setState(State::Applying);
}

void CameraAdvancedParamsWidget::saveValues()
{
    // Check that we are in the correct state.
    if (state() != State::Init)
        return;

    auto modifiedValues = m_currentValues.differenceMap(m_loadedValues);
    if (modifiedValues.isEmpty())
        return;

    const auto parameters = m_advancedParamsReader->params(m_camera);

    if (!m_cameraAvailable)
    {
        // Remove parameters not available in offline.

        for (auto it = modifiedValues.begin(); it != modifiedValues.end(); /*no increment*/)
        {
            const auto parameter = parameters.getParameterById(it.key());
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
        const auto parameter = parameters.getParameterById(it.key());
        if (!parameter.group.isEmpty())
            groups.insert(parameter.group);
    }

    const auto additionalValues = groupParameters(groups);
    for (auto it = additionalValues.cbegin(); it != additionalValues.cend(); ++it)
    {
        if (!modifiedValues.contains(it.key()))
            modifiedValues[it.key()] = it.value();
    }

    // Update state.
    if (sendSetCameraParams(m_camera, modifiedValues.toValueList()))
        setState(State::Applying);
}

bool CameraAdvancedParamsWidget::hasChanges() const
{
    return m_currentValues.differsFrom(m_loadedValues);
}

bool CameraAdvancedParamsWidget::hasItemsAvailableInOffline() const
{
    return m_advancedParamWidgetsManager->hasItemsAvailableInOffline();
}

void CameraAdvancedParamsWidget::updateCameraAvailability() {
    const bool cameraAvailable = isCameraAvailable();
    if (m_cameraAvailable == cameraAvailable)
        return;

    m_cameraAvailable = cameraAvailable;

    this->setEnabled(!m_camera.isNull());
    updateButtonsState();
    updateParameretsVisibility();
}

void CameraAdvancedParamsWidget::updateButtonsState() {
    ui->loadButton->setEnabled(m_cameraAvailable && m_state == State::Init);
}

void CameraAdvancedParamsWidget::updateParameretsVisibility()
{
    m_advancedParamWidgetsManager->updateParametersVisibility(m_cameraAvailable
        ? CameraAdvancedParamWidgetsManager::ParameterVisibility::showAll
        : CameraAdvancedParamWidgetsManager::ParameterVisibility::showOfflineOnly);
}

bool CameraAdvancedParamsWidget::isCameraAvailable() const {
    if (!m_camera)
        return false;

    // TODO: #GDM check special capability flag
    return m_camera->isOnline();
}

rest::QnConnectionPtr CameraAdvancedParamsWidget::getServerConnection(
    const QnNetworkResourcePtr& camera)
{
    if (auto cam = camera.dynamicCast<QnSecurityCamResource>())
    {
        if (const auto server = cam->getParentServer())
            return server->restConnection();
    }

    return {};
}

QnCameraAdvancedParamValueMap CameraAdvancedParamsWidget::groupParameters(
    const QSet<QString>& groups) const
{
    QnCameraAdvancedParamValueMap result;
    const auto parameters = m_advancedParamsReader->params(m_camera);
    const auto ids = parameters.allParameterIds();

    for (const auto& id: ids)
    {
        const auto parameter = parameters.getParameterById(id);
        if (groups.contains(parameter.group))
        {
            const auto parameterValue = m_advancedParamWidgetsManager->parameterValue(parameter.id);
            if (parameterValue != std::nullopt)
                result[parameter.id] = *parameterValue;
        }
    }

    return result;
}


bool CameraAdvancedParamsWidget::sendSetCameraParams(
    const QnNetworkResourcePtr& camera, const QnCameraAdvancedParamValueList& values)
{
    auto connection = getServerConnection(camera);
    if (!connection)
        return false;
    QnCameraAdvancedParamsPostBody body;
    body.cameraId = camera->getId().toString();
    for(const auto& value: values)
        body.paramValues.insert(value.id, value.value);

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, QnJsonRestResult data)
        {
            auto response = data.deserialized<QnCameraAdvancedParamValueList>();
            at_advancedParam_saved(success, handle, response);
        });

    m_paramRequestHandle = connection->postJsonResult("/api/setCameraParam", {},
        QJson::serialized(body), callback, thread());
    return m_paramRequestHandle != 0;
}

bool CameraAdvancedParamsWidget::sendGetCameraParams(const QnNetworkResourcePtr& camera, const QStringList& keys)
{
    auto connection = getServerConnection(camera);
    if (!connection)
        return false;
    QnRequestParamList params;
    params.insert("cameraId", camera->getId());
    for(const QString &param: keys)
        params.insert(param, QString());

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, QnJsonRestResult data)
        {
            auto response = data.deserialized<QnCameraAdvancedParamValueList>();
            at_advancedSettingsLoaded(success, handle, response);
        });
    m_paramRequestHandle = connection->getJsonResult("/api/getCameraParam",
        params, callback,thread());
    return true;
}

void CameraAdvancedParamsWidget::at_advancedParamChanged(const QString& id, const QString& value)
{
    // Check that we are in correct state.
    if (state() != State::Init)
        return;

    // Check parameter validity.
    const QnCameraAdvancedParams params = m_advancedParamsReader->params(m_camera);
    const QnCameraAdvancedParameter parameter = params.getParameterById(id);
    if (!parameter.isValid())
        return;

    if (parameter.isCustomControl())
    {
        // This is a custom parameter with specific API.
        sendCustomParameterCommand(parameter, value);
    }
    else if (parameterIsInstant(parameter))
    {
        // Apply instant parameters immediately.
        saveSingleValue(parameter, value);
    }
    else
    {
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
        NX_WARNING(this, "CameraAdvancedParamsWidget::at_advancedSettingsLoaded: request failed. "
            "Camera id: ", m_camera->getUniqueId());
        setState(State::Init);
        return;
    }

    /* Save loaded values in cache. */
    m_loadedValues.appendValueList(params);

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
        // TODO: #GDM
        return;
    }

    /* Update stored parameters. */
    bool needResync = false;
    const auto parameters = m_advancedParamsReader->params(m_camera);
    for (const QnCameraAdvancedParamValue& value: params)
    {
        m_loadedValues[value.id] = value.value;
        m_currentValues.remove(value.id);

        if (parameters.getParameterById(value.id).resync)
            needResync = true;
    }

    // Update values with new ones in the case of packet mode.
    if (parameters.packet_mode)
        m_advancedParamWidgetsManager->loadValues(params, /*packetMode*/ true);

    /* Update state. */
    setState(State::Init);
    emit hasChangesChanged();

    // Reload all values if one of the parameters requires resync.
    if (needResync && !parameters.packet_mode)
        loadValues();
}

CameraAdvancedParamsWidget::State CameraAdvancedParamsWidget::state() const
{
    return m_state;
}

void CameraAdvancedParamsWidget::setState(State newState)
{
    if (newState == m_state)
        return;

    m_state = newState;

    auto textByState = [](State state) {
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

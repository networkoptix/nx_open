#include "camera_advanced_params_widget.h"
#include "ui_camera_advanced_params_widget.h"
#include "camera_advanced_param_widgets_manager.h"

#include <api/media_server_connection.h>

#include <core/resource/param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/xml/camera_advanced_param_reader.h>

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

namespace nx {
namespace client {
namespace desktop {

CameraAdvancedParamsWidget::CameraAdvancedParamsWidget(QWidget* parent /*= NULL*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedParamsWidget),
    m_advancedParamsReader(new QnCachingCameraAdvancedParamsReader()),
    m_cameraAvailable(false),
    m_paramRequestHandle(0),
    m_state(State::Init),
    m_ptzSequenceId(QnUuid::createUuid())
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

void CameraAdvancedParamsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_camera == camera)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;

    if (m_camera) {
        connect(m_camera, &QnResource::statusChanged,   this, &CameraAdvancedParamsWidget::updateCameraAvailability);

        connect(m_camera, &QnResource::propertyChanged, this, [this](const QnResourcePtr &resource, const QString &key) {
            if (resource != m_camera)
                return;
            if (key == Qn::CAMERA_ADVANCED_PARAMETERS) {
                /* Re-init state if key parameter changed. */
                initialize();
            }
        });
    }

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

    /* Check that the server and camera are available. */
    auto serverConnection = getServerConnection();
    if (!serverConnection) {
        /* Notify user about error. */
        // TODO: #GDM
        return;
    }

    qDebug() << "Requesting the following parameters:" << paramIds;

    /* Update state. */
    m_paramRequestHandle = serverConnection->getParamsAsync(m_camera, paramIds, this, SLOT(at_advancedSettingsLoaded(int, const QnCameraAdvancedParamValueList &, int)) );
    setState(State::Loading);
}

void CameraAdvancedParamsWidget::saveSingleValue(const QnCameraAdvancedParamValue &value) {
    /* Check that we are in the correct state. */
    if (state() != State::Init || !m_cameraAvailable)
        return;

    /* Check that the server and camera are available. */
    auto serverConnection = getServerConnection();
    if (!serverConnection) {
        /* Notify user about error. */
        // TODO: #GDM
        return;
    }

    /* Update state. */
    m_paramRequestHandle = serverConnection->setParamsAsync(m_camera, QnCameraAdvancedParamValueList() << value, this, SLOT(at_advancedParam_saved(int, const QnCameraAdvancedParamValueList &, int)));
    setState(State::Applying);
}

void CameraAdvancedParamsWidget::sendCustomParameterCommand(const QnCameraAdvancedParameter& parameter,
    const QString& value)
{
    /* Check that we are in the correct state. */
    if (state() != State::Init || !m_cameraAvailable)
        return;

    /* Check that the server and camera are available. */
    auto serverConnection = getServerConnection();
    if (!serverConnection)
        return;

    bool ok = false;

    m_ptzSequenceNumber++;

    auto slot = SLOT(at_ptzCommandProcessed(int, const QVariant &, int));
    nx::core::ptz::Options options{nx::core::ptz::Type::configurational};

    if(parameter.writeCmd == lit("custom_zoom"))
    {
        // Expecting a single value.
        nx::core::ptz::Vector speed;
        qreal val = value.toFloat(&ok);
        if (ok)
        {
            speed.zoom = val * 0.01;
            qDebug() << "Sending custom_zoom(" << val << ")";
            serverConnection->ptzContinuousMoveAsync(
                m_camera, speed, options, m_ptzSequenceId, m_ptzSequenceNumber, this, slot);
        }
    }
    else if (parameter.writeCmd == lit("custom_ptr"))
    {
        // Expecting a value like "horisontal,vertical,rotation".
        QStringList values = value.split(L',');

        if (values.size() != 3)
            return;

        nx::core::ptz::Vector speed;

        speed.pan = values[0].toFloat(&ok);
        speed.tilt = values[1].toFloat(&ok);

        // Control provides the angle in range [-180; 180],
        // but we should send values in range [-1.0, 1.0].
        speed.rotation = qBound<qreal>(values[2].toFloat(&ok) / 180.0, -1.0, 1.0);

        qDebug() << "Sending custom_ptr(pan=" << speed.pan << ", tilt=" << speed.tilt << ", rot=" << speed.rotation << ")";

        serverConnection->ptzContinuousMoveAsync(
            m_camera,
            speed,
            options,
            m_ptzSequenceId,
            m_ptzSequenceNumber,
            this,
            slot);
    }
    else if (parameter.writeCmd == lit("custom_focus"))
    {
        // Expecting a single value.
        qreal speed = value.toFloat(&ok);
        if (ok)
        {
            speed *= 0.01;
            serverConnection->ptzContinuousFocusAsync(m_camera, speed, options, this, slot);
            qDebug() << "Sending custom_focus(" << speed << ")";
        }
    }

    /* Update state. */
    //m_paramRequestHandle = serverConnection->setParamsAsync(m_camera, QnCameraAdvancedParamValueList() << value, this, SLOT(at_advancedParam_saved(int, const QnCameraAdvancedParamValueList &, int)));
    //if (ok)
    //    setState(State::Applying);
}

void CameraAdvancedParamsWidget::at_ptzCommandProcessed(int status, const QVariant& reply, int handle)
{
    // Just an empty handler for REST response.
}


void CameraAdvancedParamsWidget::saveValues() {
    /* Check that we are in the correct state. */
    if (state() != State::Init || !m_cameraAvailable)
        return;

    /* Check that the server and camera are available. */
    auto serverConnection = getServerConnection();
    if (!serverConnection) {
        /* Notify user about error. */
        // TODO: #GDM
        return;
    }

    auto modifiedValues = m_currentValues.differenceMap(m_loadedValues);
    if (modifiedValues.isEmpty())
        return;

    QSet<QString> groups;
    const auto parameters = m_advancedParamsReader->params(m_camera);
    for (auto itr = modifiedValues.cbegin(); itr != modifiedValues.cend(); ++itr)
    {
        const auto parameter = parameters.getParameterById(itr.key());
        if (!parameter.group.isEmpty())
            groups.insert(parameter.group);
    }

    const auto additionalValues = groupParameters(groups);
    for (auto itr = additionalValues.cbegin(); itr != additionalValues.cend(); ++itr)
    {
        if (!modifiedValues.contains(itr.key()))
            modifiedValues[itr.key()] = itr.value();
    }

    /* Update state. */
    m_paramRequestHandle = serverConnection->setParamsAsync(
        m_camera,
        modifiedValues.toValueList(),
        this,
        SLOT(at_advancedParam_saved(int, const QnCameraAdvancedParamValueList &, int)));

    setState(State::Applying);
}

bool CameraAdvancedParamsWidget::hasChanges() const
{
    return m_currentValues.differsFrom(m_loadedValues);
}

void CameraAdvancedParamsWidget::updateCameraAvailability() {
    bool cameraAvailable = isCameraAvailable();
    if (m_cameraAvailable == cameraAvailable)
        return;
    m_cameraAvailable = cameraAvailable;

    this->setEnabled(!m_camera.isNull());
    updateButtonsState();
}

void CameraAdvancedParamsWidget::updateButtonsState() {
    ui->loadButton->setEnabled(m_cameraAvailable && m_state == State::Init);
}


bool CameraAdvancedParamsWidget::isCameraAvailable() const {
    if (!m_camera)
        return false;

    // TODO: #GDM check special capability flag
    return m_camera->getStatus() == Qn::Online || m_camera->getStatus() == Qn::Recording;
}


QnMediaServerConnectionPtr CameraAdvancedParamsWidget::getServerConnection() const {
    if (!m_cameraAvailable)
        return QnMediaServerConnectionPtr();

    if (QnMediaServerResourcePtr mediaServer = m_camera->getParentResource().dynamicCast<QnMediaServerResource>())
        return mediaServer->apiConnection();

    return QnMediaServerConnectionPtr();
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


void CameraAdvancedParamsWidget::at_advancedParamChanged(const QString& id, const QString& value)
{
    /* Check that we are in correct state. */
    if (state() != State::Init)
        return;

    /* Check parameter validity. */
    auto params = m_advancedParamsReader->params(m_camera);
    QnCameraAdvancedParameter parameter = params.getParameterById(id);
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
        saveSingleValue(QnCameraAdvancedParamValue(id, value));
    }
    else
    {
        /* Queue modified parameter. */
        m_currentValues[id] = value;

        emit hasChangesChanged();
    }
}

void CameraAdvancedParamsWidget::at_advancedSettingsLoaded(int status, const QnCameraAdvancedParamValueList &params, int handle) {
    /* Check that we are in the correct state. */
    if (state() != State::Loading)
        return;

    /* Check that we are waiting for this request. */
    if (handle != m_paramRequestHandle)
        return;

    /* Show error if something was not correct. */
    if (status != 0) {
        qWarning() << "CameraAdvancedParamsWidget::at_advancedSettingsLoaded: http status code is not OK: " << status
            << ". Camera id: " << m_camera->getUniqueId();
        setState(State::Init);
        return;
    }

    /* Save loaded values in cache. */
    m_loadedValues.appendValueList(params);

    /* Update modified parameters list. */
    for (const QnCameraAdvancedParamValue &value: params)
        m_currentValues.remove(value.id);

    /* Display values. */
    m_advancedParamWidgetsManager->loadValues(params);

    /* Update state. */
    setState(State::Init);

    emit hasChangesChanged();
}

void CameraAdvancedParamsWidget::at_advancedParam_saved(int status, const QnCameraAdvancedParamValueList &params, int handle) {
    /* Check that we are in the correct state. */
    if (state() != State::Applying)
        return;

    /* Check that we are waiting for this request. */
    if (handle != m_paramRequestHandle)
        return;

    /* Show error if something was not correct. */
    if (status != 0) {
        // TODO: #GDM
        return;
    }

    /* Update stored parameters. */
    bool needResync = false;
    for (const QnCameraAdvancedParamValue &value: params) {
        m_loadedValues[value.id] = m_currentValues[value.id];
        m_currentValues.remove(value.id);

        const auto parameters = m_advancedParamsReader->params(m_camera);
        if (parameters.getParameterById(value.id).resync)
            needResync = true;
    }

    /* Update state. */
    setState(State::Init);
    emit hasChangesChanged();

    /* Reload all values if one of parameters requires resync. */
    if (needResync)
        loadValues();
}

CameraAdvancedParamsWidget::State CameraAdvancedParamsWidget::state() const {
    return m_state;
}

void CameraAdvancedParamsWidget::setState(State newState) {
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

} // namespace desktop
} // namespace client
} // namespace nx

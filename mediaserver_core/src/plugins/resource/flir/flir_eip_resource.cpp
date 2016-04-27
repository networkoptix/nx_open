#include "flir_eip_resource.h"
#include "common/common_module.h"
#include <utils/common/synctime.h>
#include "core/resource_management/resource_data_pool.h"
#include "utils/serialization/json.h"

const QString QnFlirEIPResource::MANUFACTURE(lit("FLIR"));

namespace {
    const int kIOCheckTimeout = 3000;
    const int kAlarmCheckTimeout = 300;
    const QString kAlarmsCountParamName("alarmsCount");
}

QnFlirEIPResource::QnFlirEIPResource() :
    m_inputPortMonitored(false),
    m_currentCheckingPortNumber(0),
    m_currentAlarmMonitoringState(FlirAlarmMonitoringState::ReadyToCheckAlarm)
{
}

QnFlirEIPResource::~QnFlirEIPResource()
{
    stopInputPortMonitoringAsync();
}

QByteArray QnFlirEIPResource::PASSTHROUGH_EPATH()
{    
    return MessageRouterRequest::buildEPath(
        FlirEIPClass::kPassThrough,
        0x01);
}

CameraDiagnostics::Result QnFlirEIPResource::initInternal()
{
    QnSecurityCamResource::initInternal();

    m_eipClient = std::make_shared<SimpleEIPClient>(QHostAddress(getHostAddress()));
    m_eipAsyncClient = std::make_shared<EIPAsyncClient>(QHostAddress(getHostAddress()));
    m_outputEipAsyncClient = std::make_shared<EIPAsyncClient>(QHostAddress(getHostAddress()));
    m_alarmsEipAsyncClient = std::make_shared<EIPAsyncClient>(QHostAddress(getHostAddress()));

    m_eipClient->connect();
    m_eipClient->registerSession();

    setCameraCapabilities(
        Qn::PrimaryStreamSoftMotionCapability |
        Qn::RelayInputCapability |
        Qn::RelayOutputCapability );

    initializeIO();

    fetchAndSetAdvancedParameters();
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnFlirEIPResource::hasDualStreaming() const
{
    return false;
}

QString QnFlirEIPResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnFlirEIPResource::setIframeDistance(int, int)
{

}

QnAbstractStreamDataProvider* QnFlirEIPResource::createLiveDataProvider()
{
    return new QnRtpStreamReader(toSharedPointer(this), "avc");
}

QString QnFlirEIPResource::getParamDataType(const QnCameraAdvancedParameter &param) const
{
    return param.readCmd.isEmpty() ?
        FlirDataType::Ascii : param.readCmd.toLower();
}

quint8 QnFlirEIPResource::getServiceCodeByType(const QString& type, const ParamsRequestMode& mode) const
{
    if(type == FlirDataType::Ascii)
        return mode == ParamsRequestMode::SetMode ?
            FlirEIPPassthroughService::kWriteAscii :
            FlirEIPPassthroughService::kReadAscii;
    else if (type == FlirDataType::Int)
        return  mode == ParamsRequestMode::SetMode ?
            FlirEIPPassthroughService::kWriteInt32 :
            FlirEIPPassthroughService::kReadInt32;
    else if(type == FlirDataType::Double)
        return  mode == ParamsRequestMode::SetMode ?
            FlirEIPPassthroughService::kWriteDouble :
            FlirEIPPassthroughService::kReadDouble;
    else if(type == FlirDataType::Bool)
        return  mode == ParamsRequestMode::SetMode ?
            FlirEIPPassthroughService::kWriteBool :
            FlirEIPPassthroughService::kReadBool;

    return FlirEIPPassthroughService::kReadAscii;
}

bool QnFlirEIPResource::isPassthroughParam(const QnCameraAdvancedParameter &param) const
{
    return param.id.startsWith(".");

}

CIPPath QnFlirEIPResource::parseParamCIPPath(const QnCameraAdvancedParameter &param) const
{
    CIPPath path;
    bool ok;
    auto cleanPath = param.id.split("-");
    auto pathChunks = cleanPath[0].split(".");

    path.classId = pathChunks[0].toUInt(&ok, 16);
    path.instanceId = pathChunks[1].toUInt(&ok, 16);
    if(pathChunks.size() == 3)
        path.attributeId = pathChunks[2].toUInt(&ok, 16);

    return path;
}

QByteArray QnFlirEIPResource::encodePassthroughVariableName(const QString& variableName) const
{
    QByteArray encodedData;

    auto variableNameEncoded = variableName.toLatin1();
    encodedData[0] = variableNameEncoded.size();
    encodedData.append(variableNameEncoded);

    return encodedData;
}

MessageRouterRequest QnFlirEIPResource::buildPassthroughGetRequest(quint8 serviceCode, const QString &variableName) const
{
    MessageRouterRequest request;
    request.serviceCode = serviceCode;
    request.pathSize = 2;
    request.epath = PASSTHROUGH_EPATH();
    request.data = encodePassthroughVariableName(variableName);

    return request;
}

MessageRouterRequest QnFlirEIPResource::buildEIPGetRequest(const QnCameraAdvancedParameter &param) const
{
    MessageRouterRequest request;

    if(isPassthroughParam(param))
    {
        const auto type = getParamDataType(param);

        request.serviceCode = getServiceCodeByType(type, ParamsRequestMode::GetMode);
        request.pathSize = 2;
        request.epath = PASSTHROUGH_EPATH();
        request.data = encodeGetParamData(param);
    }
    else
    {
        CIPPath cipPath = parseParamCIPPath(param);
        request.serviceCode = CIPServiceCode::kGetAttributeSingle;
        request.pathSize = 3;
        request.epath = MessageRouterRequest::buildEPath(
            cipPath.classId,
            cipPath.instanceId,
            cipPath.attributeId);
    }


    return request;
}

MessageRouterRequest QnFlirEIPResource::buildEIPSetRequest(const QnCameraAdvancedParameter &param, const QString &value) const
{
    MessageRouterRequest request;

    if(isPassthroughParam(param))
    {
        const auto type = getParamDataType(param);
        request.serviceCode = getServiceCodeByType(type, ParamsRequestMode::SetMode);
        request.pathSize = 2;
        request.epath = PASSTHROUGH_EPATH();
        request.data = encodeSetParamData(param, value);

    }
    else
    {
        CIPPath cipPath= parseParamCIPPath(param);
        request.serviceCode = CIPServiceCode::kSetAttributeSingle;
        request.pathSize = 3;
        request.epath = MessageRouterRequest::buildEPath(
            cipPath.classId,
            cipPath.instanceId,
            cipPath.attributeId);
        request.data = encodeSetParamData(param, value);
    }

    return request;
}

QByteArray QnFlirEIPResource::encodeGetParamData(const QnCameraAdvancedParameter &param) const
{
    return encodePassthroughVariableName(param.id);
}

QByteArray QnFlirEIPResource::encodeSetParamData(const QnCameraAdvancedParameter &param, const QString &value) const
{
    QByteArray encoded;
    const auto type = getParamDataType(param);

    if(isPassthroughParam(param))
    {
        const auto variableNameEncoded = param.id.toLatin1();
        encoded[0] = variableNameEncoded.size();
        encoded.append(variableNameEncoded);
    }

    QString val;

    val = value;
    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        val = param.toInternalRange(value);

    if(type == FlirDataType::Ascii)
    {
        auto valueBuf = val.toLatin1();
        encoded[encoded.size()] = static_cast<quint8>(valueBuf.size());
        encoded.append(valueBuf);
    }
    else if(type == FlirDataType::Bool)
    {
        quint8 boolVal = val == lit("true") ? 0x01 : 0x00;
        encoded[encoded.size()] = boolVal;
    }
    else if(type == FlirDataType::Double)
    {
        QByteArray buf;
        QDataStream stream(&buf, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        stream << val.toFloat();
        encoded.append(buf);
    }
    else if(type == FlirDataType::Int || type == FlirDataType::Short)
    {
        QByteArray buf;
        QDataStream stream(&buf, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        if(type == FlirDataType::Int)
            stream << static_cast<quint32>(val.toUInt());
        else
            stream << static_cast<quint8>(val.toUInt());

        encoded.append(buf);
    }

    return encoded;
}

QString QnFlirEIPResource::parseAsciiEIPResponse(const MessageRouterResponse& response) const
{
    if(response.data.isEmpty())
        return QString();

    auto dataLength = response.data[0];
    return response.data.mid(1, dataLength);
}

quint32 QnFlirEIPResource::parseInt32EIPResponse(const MessageRouterResponse& response) const
{
    quint32 num;
    QDataStream stream(const_cast<QByteArray*>(&response.data), QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream >> num;
    return num;
}


QString QnFlirEIPResource::parseEIPResponse(const MessageRouterResponse &response, const QnCameraAdvancedParameter &param) const
{
    const auto type = getParamDataType(param);
    const auto rawData = response.data;
    QString data;

    if(rawData.isEmpty())
        return QString();

    if(type == FlirDataType::Ascii)
    {
        data = parseAsciiEIPResponse(response);
    }
    else if(type == FlirDataType::Bool)
    {
        data = rawData[0] == 0x01 ? lit("true") : lit("false");
    }
    else if(type == FlirDataType::Int)
    {
        data = QString::number(parseInt32EIPResponse(response));
    }
    else if(type == FlirDataType::Short)
    {
        data = QString::number(rawData[0]);
    }
    else if(type == FlirDataType::Double)
    {
        float num;
        QDataStream stream(const_cast<QByteArray*>(&rawData), QIODevice::ReadOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        stream >> num;

        data = (param.dataType == QnCameraAdvancedParameter::DataType::Enumeration ?
            QString::number(num) :
            QString::number(static_cast<int>(num)));
    }

    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        data = param.fromInternalRange(data);

    return data;
}

bool QnFlirEIPResource::commitParam(const QnCameraAdvancedParameter &param)
{
    const auto commitCmd = param.writeCmd;
    if(commitCmd.isEmpty())
        return false;

    MessageRouterRequest request;
    request.serviceCode = FlirEIPPassthroughService::kWriteBool;
    request.epath = PASSTHROUGH_EPATH();
    request.pathSize = 2;
    request.data[0] = commitCmd.size();
    request.data.append(commitCmd.toLatin1());
    request.data[request.data.size()] = 0x01;
    auto response = m_eipClient->doServiceRequest(request);
    return true;
}

bool  QnFlirEIPResource::handleButtonParam(const QnCameraAdvancedParameter &param)
{
    CIPPath path = parseParamCIPPath(param);
    QByteArray data;

    if(!param.writeCmd.isEmpty())
        data = QByteArray::fromHex(param.writeCmd.toLatin1());

    auto code = param.readCmd == lit("reset") ?
        CIPServiceCode::kReset :
        CIPServiceCode::kSetAttributeSingle;

    MessageRouterRequest request;
    request.epath = MessageRouterRequest::buildEPath(
        path.classId,
        path.instanceId,
        path.attributeId);
    request.serviceCode = code;
    request.data = data;
    request.pathSize = (path.attributeId == 0 ? 2 : 3);

    auto response = m_eipClient->doServiceRequest(request);
    return (response.generalStatus == CIPGeneralStatus::kSuccess);
}

bool QnFlirEIPResource::getParamPhysical(const QString &id, QString &value)
{
    const auto param = m_advancedParameters.getParameterById(id);

    if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
        return false;

    const auto eipRequest = buildEIPGetRequest(param);
    const auto response = m_eipClient->doServiceRequest(eipRequest);

    if(response.generalStatus != CIPGeneralStatus::kSuccess)
        return false;

    value = parseEIPResponse(response, param);

    return true;
}

bool QnFlirEIPResource::setParamPhysical(const QString &id, const QString &value)
{
    bool res = false;
    const auto param  =  m_advancedParameters.getParameterById(id);

    if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
        return handleButtonParam(param);

    const auto eipRequest = buildEIPSetRequest(param, value);
    const auto response = m_eipClient->doServiceRequest(eipRequest);

    res = (response.generalStatus == CIPGeneralStatus::kSuccess);

    if(!res)
        return res;

    if(!param.writeCmd.isEmpty())
        res = commitParam(param);

    return res;
}

void QnFlirEIPResource::fetchAndSetAdvancedParameters()
{
    QnMutexLocker lock( &m_physicalParamsMutex );
    m_advancedParameters.clear();

    auto templateFile = getAdvancedParametersTemplate();
    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplateFromFile(
            params,
            lit(":/camera_advanced_params/") + templateFile))
    {
        return;
    }

    QSet<QString> supportedParams = calculateSupportedAdvancedParameters(params);
    m_advancedParameters = params.filtered(supportedParams);
    QnCameraAdvancedParamsReader::setParamsToResource(
        this->toSharedPointer(),
        m_advancedParameters);
}

QString QnFlirEIPResource::getAdvancedParametersTemplate() const
{
    return lit("flir-") + getModel().toLower() + lit(".xml");
}

bool QnFlirEIPResource::loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& templateFilename)
{
    QFile paramsTemplateFile(templateFilename);
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);
#ifdef _DEBUG
    if (!result)
        qWarning() << "Error while parsing xml" << templateFilename;
#endif
    return result;
}

QSet<QString> QnFlirEIPResource::calculateSupportedAdvancedParameters(const QnCameraAdvancedParams &allParams) const
{
    return allParams.allParameterIds();
}

bool QnFlirEIPResource::startInputPortMonitoringAsync(std::function<void (bool)> &&completionHandler)
{
    QnMutexLocker lock(&m_ioMutex);
    qDebug() << "Starting  input port monitoring";

    if(m_inputPortMonitored)
        return false;

    QObject::connect(
        m_eipAsyncClient.get(), &EIPAsyncClient::done,
        this, &QnFlirEIPResource::checkInputPortStatusDone,
        Qt::DirectConnection);

    m_inputPortMonitored = true;
    m_checkInputPortStatusTimerId = TimerManager::instance()->addTimer(
        std::bind(&QnFlirEIPResource::checkInputPortStatus, this, std::placeholders::_1),
        kIOCheckTimeout );

    startAlarmMonitoringAsync();

    return true;
}

bool QnFlirEIPResource::startAlarmMonitoringAsync()
{
    if (m_alarmStates.empty())
        return false;

    qDebug() << "Starting alarm monitoring";
    m_currentAlarmMonitoringState = FlirAlarmMonitoringState::ReadyToCheckAlarm;
    m_currentCheckingAlarmNumber = 0;

    QObject::connect(
        m_alarmsEipAsyncClient.get(), &EIPAsyncClient::done,
        this, &QnFlirEIPResource::routeAlarmMonitoringFlow,
        Qt::DirectConnection);

    m_checkAlarmStatusTimerId = TimerManager::instance()->addTimer(
        std::bind(&QnFlirEIPResource::checkAlarmStatus, this, std::placeholders::_1),
        kAlarmCheckTimeout );

    return true;
}

bool QnFlirEIPResource::isMeasFunc(const QnIOPortData &port) const
{
    return port.id.startsWith(lit("alarm"))
        && port.id.split('_').size() == 3;
}

QString QnFlirEIPResource::getMeasFuncType(const QnIOPortData& port) const
{
    auto split = port.id.split('_');

    if (split.size() != 3)
        return QString();

    return split[1];
}

bool QnFlirEIPResource::findAlarmInputByTypeAndId(int id, const QString& type, QnIOPortData& portFound) const
{
    for(const auto& port: m_inputPorts)
    {
        if(port.id == lit("alarm_") + type + lit("_") + QString::number(id))
        {
            portFound = port;
            return true;
        }
    }

    return false;
}

void QnFlirEIPResource::initializeIO()
{
    QnMutexLocker lock(&m_ioMutex);
    auto resData = qnCommon->dataPool()->data(MANUFACTURE, getModel());
    auto portList = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);
    auto alarmsCount = resData.value<int>(kAlarmsCountParamName);

    for (const auto& port: portList)
        if (port.portType == Qn::PT_Input)
        {
            m_inputPortStates.push_back(false);
            m_inputPorts.push_back(port);
            if (isMeasFunc(port))
                m_supportedMeasFuncs << getMeasFuncType(port);
        }
        else if (port.portType == Qn::PT_Output)
        {
            m_outputPorts.push_back(port);
        }

    for (size_t i=0; i<alarmsCount; i++)
        m_alarmStates.push_back(false);

    setIOPorts(portList);
}

QnIOPortDataList QnFlirEIPResource::getRelayOutputList() const
{
    QnMutexLocker lock(&m_ioMutex);
    return m_outputPorts;
}

QnIOPortDataList QnFlirEIPResource::getInputPortList() const
{
    QnMutexLocker lock(&m_ioMutex);
    return m_inputPorts;
}

void QnFlirEIPResource::stopInputPortMonitoringAsync()
{
    QnMutexLocker lock(&m_ioMutex);
    m_inputPortMonitored = false;
}

bool QnFlirEIPResource::isInputPortMonitored() const
{
    QnMutexLocker lock(&m_ioMutex);
    return m_inputPortMonitored;
}

MessageRouterRequest QnFlirEIPResource::buildEIPOutputPortRequest(const QString &portId, bool portState) const
{
    MessageRouterRequest request;

    request.serviceCode = CIPServiceCode::kSetAttributeSingle;
    request.pathSize = 3;
    request.epath = MessageRouterRequest::buildEPath(
        FlirEIPClass::kPhysicalIO,
        0x01,
        getOutputPortCIPAttributeById(portId));

    request.data[0] = static_cast<quint8>(portState);

    return request;
}

bool QnFlirEIPResource::setRelayOutputState(const QString &outputID, bool activate, unsigned int autoResetTimeoutMS)
{
    QString id = outputID.isEmpty() ?
        m_outputPorts[0].id :
        outputID;

    auto req = buildEIPOutputPortRequest(id, activate);
    return m_outputEipAsyncClient->doServiceRequestAsync(req);
}

void QnFlirEIPResource::checkInputPortStatus(quint64 timerId)
{
    QnMutexLocker lock(&m_ioMutex);
    if(timerId != m_checkInputPortStatusTimerId)
        return;

    m_checkInputPortStatusTimerId = 0;
    if( !m_inputPortMonitored )
        return;

    MessageRouterRequest request;
    request.serviceCode =
        CIPServiceCode::kGetAttributeSingle;
    request.pathSize = 3;
    request.epath = MessageRouterRequest::buildEPath(
        FlirEIPClass::kPhysicalIO,
        0x01,
        getInputPortCIPAttribute(m_currentCheckingPortNumber));

    if(!m_eipAsyncClient->doServiceRequestAsync(request))
    {
        m_checkInputPortStatusTimerId = TimerManager::instance()->addTimer(
            std::bind(&QnFlirEIPResource::checkInputPortStatus, this, std::placeholders::_1),
            kIOCheckTimeout );
    }
}

void QnFlirEIPResource::checkInputPortStatusDone()
{
    QnMutexLocker lock(&m_ioMutex);
    auto response =  m_eipAsyncClient->getResponse();
    bool portState = response.data[0] != char(0);

    if (portState != m_inputPortStates[m_currentCheckingPortNumber]
        && response.generalStatus == CIPGeneralStatus::kSuccess)
    {
        m_inputPortStates[m_currentCheckingPortNumber] = portState;
        lock.unlock();
        emit cameraInput(
            toSharedPointer(),
            m_inputPorts[m_currentCheckingPortNumber].id,
            portState,
            qnSyncTime->currentUSecsSinceEpoch());
        lock.relock();
    }

    if (++m_currentCheckingPortNumber == m_inputPortStates.size())
        m_currentCheckingPortNumber = 0;

    if (m_currentCheckingPortNumber == 0)
    {
        m_checkInputPortStatusTimerId = TimerManager::instance()->addTimer(
            std::bind(&QnFlirEIPResource::checkInputPortStatus, this, std::placeholders::_1),
            kIOCheckTimeout );
    }
    else
    {
        lock.unlock();
        checkInputPortStatus(m_checkInputPortStatusTimerId);
    }
}

void QnFlirEIPResource::routeAlarmMonitoringFlow()
{
    if (m_currentAlarmMonitoringState == FlirAlarmMonitoringState::CheckingAlaramState)
        checkAlarmStatusDone();
    else if (m_currentAlarmMonitoringState == FlirAlarmMonitoringState::WaitingForMeasFuncType)
        getAlarmMeasurementFuncTypeDone();
    else if (m_currentAlarmMonitoringState == FlirAlarmMonitoringState::WaitingForMeasFuncId)
        getAlarmMeasurementFuncIdDone();
}

void QnFlirEIPResource::scheduleNextAlarmCheck()
{
    m_currentAlarmMonitoringState = FlirAlarmMonitoringState::ReadyToCheckAlarm;
    m_checkAlarmStatusTimerId = TimerManager::instance()->addTimer(
        std::bind(&QnFlirEIPResource::checkAlarmStatus, this, std::placeholders::_1),
        kAlarmCheckTimeout );
}

void QnFlirEIPResource::checkAlarmStatus(quint64 timerId)
{
    if (timerId != m_checkAlarmStatusTimerId)
        return;

    m_checkAlarmStatusTimerId = 0;
    if (!m_inputPortMonitored)
        return;

    qDebug() << "Checking alarm status" << m_currentCheckingAlarmNumber + 1;

    MessageRouterRequest request;
    const quint8 kAlarmAttributeCode = 0x01;

    m_currentCheckingAlarmNumber++;
    if (m_currentCheckingAlarmNumber == m_alarmStates.size())
        m_currentCheckingAlarmNumber = 0;

    request.serviceCode = CIPServiceCode::kGetAttributeSingle;
    request.pathSize = 3;
    request.epath = MessageRouterRequest::buildEPath(
        FlirEIPClass::kAlarmSettings,
        m_currentCheckingAlarmNumber + 1,
        kAlarmAttributeCode);

    m_currentAlarmMonitoringState = FlirAlarmMonitoringState::CheckingAlaramState;
    if (!m_alarmsEipAsyncClient->doServiceRequestAsync(request))
        scheduleNextAlarmCheck();
}


void QnFlirEIPResource::checkAlarmStatusDone()
{
    auto response = m_alarmsEipAsyncClient->getResponse();
    bool alarmState = response.data[0] != char(0);

    if (alarmState != m_alarmStates[m_currentCheckingAlarmNumber]
        && response.generalStatus == CIPGeneralStatus::kSuccess)
    {
        m_alarmStates[m_currentCheckingAlarmNumber] = alarmState;
        qDebug() << "Checking alarm measure function type";
        getAlarmMeasurementFuncType();
    }
    else
    {
        qDebug() << "Alarm status is the same (or error occured) checking next alarm";
        scheduleNextAlarmCheck();
    }
}

void QnFlirEIPResource::getAlarmMeasurementFuncType()
{
    QString measFuncTypeAttr = lit(".image.sysimg.alarms.measfunc.")
        + QString::number(m_currentCheckingAlarmNumber + 1)
        + lit(".measFuncType");

    qDebug() << "Get meas func type varname"  << measFuncTypeAttr;

    MessageRouterRequest request =
        buildPassthroughGetRequest(FlirEIPPassthroughService::kReadAscii, measFuncTypeAttr);

    m_currentAlarmMonitoringState = FlirAlarmMonitoringState::WaitingForMeasFuncType;
    if (!m_alarmsEipAsyncClient->doServiceRequestAsync(request))
        scheduleNextAlarmCheck();
}

void QnFlirEIPResource::getAlarmMeasurementFuncTypeDone()
{
    auto response = m_alarmsEipAsyncClient->getResponse();
    auto measFuncType = parseAsciiEIPResponse(response);

    qDebug() << "Got alarm measurment func type:"
        << measFuncType << response.data.toHex();

    if (response.generalStatus == CIPGeneralStatus::kSuccess
        && m_supportedMeasFuncs.contains(measFuncType))
    {
        m_currentAlarmMonitoringState = FlirAlarmMonitoringState::WaitingForMeasFuncId;
        m_currentCheckingMeasFuncType = measFuncType;
        getAlarmMeasurementFuncId();
    }
    else
    {
        scheduleNextAlarmCheck();
    }
}


void QnFlirEIPResource::getAlarmMeasurementFuncId()
{
    QString measFuncIdAttr = lit(".image.sysimg.alarms.measfunc.")
        + QString::number(m_currentCheckingAlarmNumber + 1)
        + lit(".measFuncId");

    qDebug() << "Meas func id varname" << measFuncIdAttr;

    MessageRouterRequest request =
        buildPassthroughGetRequest(FlirEIPPassthroughService::kReadInt32, measFuncIdAttr);

    m_currentAlarmMonitoringState = FlirAlarmMonitoringState::WaitingForMeasFuncId;
    if (!m_alarmsEipAsyncClient->doServiceRequestAsync(request))
        scheduleNextAlarmCheck();
}

void QnFlirEIPResource::getAlarmMeasurementFuncIdDone()
{
    auto response = m_alarmsEipAsyncClient->getResponse();
    auto id = parseInt32EIPResponse(response);

    QnIOPortData port;
    if (response.generalStatus == CIPGeneralStatus::kSuccess
        && findAlarmInputByTypeAndId(id, m_currentCheckingMeasFuncType, port))
    {
        qDebug() << "Emitting signal cameraInput" << port.id;

        emit cameraInput(
            toSharedPointer(),
            port.id,
            m_alarmStates[m_currentCheckingAlarmNumber],
            qnSyncTime->currentUSecsSinceEpoch());
    }

    scheduleNextAlarmCheck();
}


quint8 QnFlirEIPResource::getInputPortCIPAttribute(size_t portNum) const
{
    Q_ASSERT(portNum < m_inputPorts.size());
    return static_cast<quint8>(m_inputPorts[portNum].id.toUInt());
}

quint8 QnFlirEIPResource::getOutputPortCIPAttributeById(const QString &portId) const
{
    return static_cast<quint8>(portId.toUInt());
}

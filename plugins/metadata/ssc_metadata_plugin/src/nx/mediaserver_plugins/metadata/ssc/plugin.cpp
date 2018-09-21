#include "plugin.h"

#include <sstream>
#include <iomanip>

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtSerialPort/QSerialPortInfo>
#include <QDir>

#include <nx/fusion/model_functions.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/kit/ini_config.h>

#include "manager.h"
#include "log.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

namespace {

static const char* const kPluginName = "SSC metadata plugin";

static const QString kSerialPortsIniFilename = "SerialPorts.ini";

static const QByteArray defaultConfiguration = R"json(\
{
    "useAllPorts": false,
    "values":
    [
        "COM1",
        "COM2"
    ]
}
)json";

// The following constants are from "SSC System Specification" (page 5).
static const unsigned int kCommandLength = 4;
static const char kSTX = 0x02; //< start byte
static const char kETX = 0x03; //< stop byte
static const char kMinDigitCode = 0x30;
static const char kMinLogicalId = 1;
static const char kMaxLogicalId = 16;
static const char kResetCommand[] = {kSTX, 0x40, 0x40, kETX, '\0'};

static const std::string resultStatus[] = { "Failed", "Succeeded" };

void logReceivedBytes(const char* message, const QByteArray& data, int len)
{
    NX_ASSERT(data.size() >= len);
    std::stringstream tempStream;
    for (int i = 0; i < len; ++i)
    {
        tempStream << std::hex << "0x" << std::setfill('0') << std::setw(2)
            << int(data[i]) << " ";
    }
    NX_PRINT << message << tempStream.str();
}

int extractLogicalId(const QByteArray& data)
{
    NX_ASSERT(data.size() >= kCommandLength);
    return (data[1] - kMinDigitCode) * 10 + (data[2] - kMinDigitCode);
};

/** Check if data begins with a correct command. */
bool isCorrectCommand(const QByteArray& data)
{
    NX_ASSERT(data.size() >= kCommandLength);
    const bool commandEnvelopeIsCorrect = (data[0] == kSTX && data[3] == kETX);

    const char b1 = data[1];
    const char b2 = data[2];
    const bool commandBodyIsCorrect =
        (b1 == 0x40 && b2 == 0x40)
        || (b1 == 0x30 && 0x31 <= b2 && b2 <= 0x39)
        || (b1 == 0x31 && 0x30 <= b2 && b2 <= 0x36);
    if (commandEnvelopeIsCorrect && commandBodyIsCorrect)
    {
        logReceivedBytes("Input buffer contains correct command: ", data, kCommandLength);
        return true;
    }
    else
    {
        NX_PRINT << "Input buffer contains garbage";
        return false;
    }
}

void removeInvalidBytes(QByteArray& data)
{
    // This function is called, if data begins with incorrect command,
    // so we remove first byte in all cases,
    // and continue to delete bytes until we meet 'start byte'.
    int startByteIndex = 1;
    while (startByteIndex < data.size() && data[startByteIndex] != kSTX)
        ++startByteIndex;

    logReceivedBytes("Incorrect bytes ignored: ", data, startByteIndex);

    data.remove(0, startByteIndex);
}

bool isCorrectLogicalId(int cameraLogicalId)
{
    return cameraLogicalId >= kMinLogicalId && cameraLogicalId <= kMaxLogicalId;
};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
{
    static const char* const kManifestResourceName = ":/ssc/manifest.json";
    static const char* const kManifestFilename = "plugins/ssc/manifest.json";
    static const QString kCameraSpecifyCommandInternalName = "camera specify command";
    static const QString kResetCommandInternalName = "reset command";

    bool result = false;
    QFile f(kManifestResourceName);
    result = f.open(QFile::ReadOnly);
    NX_PRINT << "Tried to read manifest file " << kManifestResourceName << ": "
        << resultStatus[result];

    if (result)
    {
        m_manifest = f.readAll();
        NX_PRINT << "Read " << m_manifest.size()
            << " byte(s) from manifest resource" << kManifestResourceName;
    }

    {
        QFile file(kManifestFilename);
        if (file.open(QFile::ReadOnly))
        {
            NX_PRINT << "Switch to external manifest file "
                << QFileInfo(file).absoluteFilePath().toStdString();
            m_manifest = file.readAll();
            NX_PRINT << "Read " << m_manifest.size()
                << " byte(s) read from manifest file" << kManifestFilename;
        }
    }
    f.close();
    m_typedManifest = QJson::deserialized<AnalyticsDriverManifest>(m_manifest);

    int eventCount = m_typedManifest.outputEventTypes.size();
    NX_PRINT << "Found "<< eventCount << " event(s) in the loaded manifest";

    for (const auto& eventType: m_typedManifest.outputEventTypes)
    {
        if (eventType.internalName == kCameraSpecifyCommandInternalName)
            cameraEventType = eventType;
        else if (eventType.internalName == kResetCommandInternalName)
            resetEventType = eventType;
    }

    readAllowedPortNames();
    initPorts();
}

void Plugin::readAllowedPortNames()
{
    const QString iniPath = QString::fromUtf8(nx::kit::IniConfig::iniFilesDir());
    QDir dir(iniPath);
    if (!dir.exists())
        dir.mkpath(".");

    const QString iniFullName = iniPath + kSerialPortsIniFilename;
    QFile iniFile(iniFullName);
    const bool opened = iniFile.open(QFile::ReadOnly);
    NX_PRINT << "Tried to open " << iniFullName << ": " << resultStatus[opened];
    if (opened)
    {
        QByteArray iniData = iniFile.readAll();
        NX_PRINT << "Read " << iniData.size() << " byte(s) form ini file " << iniFullName;
        m_allowedPortNames = QJson::deserialized<AllowedPortNames>(iniData);
        QString portsNames;
        if (m_allowedPortNames.useAllPorts)
        {
            portsNames = "All";
        }
        else if (m_allowedPortNames.values.isEmpty())
        {
            portsNames = "None";
        }
        else
        {
            for (const QString& value: m_allowedPortNames.values)
                portsNames = portsNames + value + " ";
        }
        NX_PRINT << "Allowed serial ports: " << portsNames;
    }
    else
    {
        NX_PRINT << "Failed to read configuration file. Default configuration will be used";
        const bool created = iniFile.open(QFile::WriteOnly);
        if (created)
        {
            NX_PRINT << "Failed to create configuration file with default settings";
        }
        else
        {
            iniFile.write(defaultConfiguration);
            m_allowedPortNames = QJson::deserialized<AllowedPortNames>(defaultConfiguration);
            NX_PRINT << "New configuration file with default settings created";
        }
    }
}

void Plugin::initPorts()
{
    if (m_allowedPortNames.useAllPorts)
    {
        NX_PRINT << "Enumerating all accessible serial ports";
        const QList<QSerialPortInfo> serialPortInfoItems = QSerialPortInfo::availablePorts();
        m_receivedDataList.reserve(serialPortInfoItems.size());
        m_serialPortList.reserve(serialPortInfoItems.size());
        NX_PRINT << "Found " << serialPortInfoItems.size() << " serial port(s)";

        NX_PRINT << "Initializing all accessible serial ports";
        for (int i = 0; i < serialPortInfoItems.size(); ++i)
            initPort(serialPortInfoItems[i].portName(), i);
    }
    else if (!m_allowedPortNames.values.isEmpty())
    {
        NX_PRINT << "Initializing serial ports from the configuration";
        for (int i = 0; i < m_allowedPortNames.values.size(); ++i)
            initPort(m_allowedPortNames.values[i], i);
    }
}

void Plugin::initPort(const QString& portName, int index)
{
    m_receivedDataList.push_back(QByteArray());
    m_serialPortList.push_back(new QSerialPort());
    configureSerialPort(m_serialPortList.back(), portName, index);
}

void Plugin::configureSerialPort(QSerialPort* port, const QString& name, int index)
{
    bool result = false;
    NX_PRINT << "Serial port " << name.toStdString() << " configuration started";
    port->setPortName(name);

    result = port->open(QIODevice::ReadOnly);
    NX_PRINT << "Tried to open: " << resultStatus[result];
    if (!result)
        return;

    result = port->setBaudRate(QSerialPort::Baud9600, QSerialPort::Input);
    NX_PRINT << "Tried to set 9600 baud: " << resultStatus[result];

    result = port->setDataBits(QSerialPort::Data8);
    NX_PRINT << "Tried to set 8 data bits: " << resultStatus[result];

    result = port->setFlowControl(QSerialPort::NoFlowControl);
    NX_PRINT << "Tried to set no flow control: " << resultStatus[result];

    result = port->setParity(QSerialPort::NoParity);
    NX_PRINT << "Tried to set no parity: " << resultStatus[result];

    result = port->setStopBits(QSerialPort::OneStop);
    NX_PRINT << "Tried to set 1 stop bit: " << resultStatus[result];

    QObject::connect(port, &QSerialPort::readyRead, [this, index](){onDataReceived(index);});
    NX_PRINT << "Serial port " << name.toStdString() << " configuration finished";
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Plugin)
    {
        addRef();
        return static_cast<Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* Plugin::name() const
{
    return kPluginName;
}

void Plugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

void Plugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
}

void Plugin::setLocale(const char* /*locale*/)
{
}

void Plugin::onDataReceived(int index)
{
    static const int kResetId = extractLogicalId(QByteArray(kResetCommand));

    QMutexLocker locker(&m_cameraMutex);

    QByteArray& receivedData = m_receivedDataList[index];
    const QByteArray dataChunk = m_serialPortList[index]->readAll();
    NX_PRINT << dataChunk.size() << " bytes received on port "
        << m_serialPortList[index]->portName().toStdString() <<":";
    logReceivedBytes("", dataChunk, dataChunk.size());

    receivedData += dataChunk;

    while (receivedData.size() >= kCommandLength)
    {
        while(receivedData.size() >= kCommandLength && !isCorrectCommand(receivedData))
            removeInvalidBytes(receivedData);

        if (receivedData.size() >= kCommandLength)
        {
            int cameraLogicalId = extractLogicalId(receivedData);
            if (cameraLogicalId == kResetId)
            {
                for (int cameraLogicalId : m_activeCameras)
                {
                    auto it = m_cameraMap.find(cameraLogicalId);
                    if (it != m_cameraMap.end())
                        it.value()->sendEventPacket(resetEventType);
                }
                m_activeCameras.clear();
            }
            else if (isCorrectLogicalId(cameraLogicalId))
            {
                auto it = m_cameraMap.find(cameraLogicalId);
                if (it != m_cameraMap.end())
                {
                    m_activeCameras << cameraLogicalId;
                    it.value()->sendEventPacket(cameraEventType);
                }
                else
                {
                    NX_PRINT << "Corresponding manager is not fetching metadata";
                }
            }
            else
            {
                NX_ASSERT(false, "Command is correct, but logical id is not");
            }
            receivedData.remove(0, kCommandLength);
        }
    }
}

void Plugin::registerCamera(int cameraLogicalId, Manager* manager)
{
    QMutexLocker locker(&m_cameraMutex);
    m_cameraMap.insert(cameraLogicalId, manager);
}

void Plugin::unregisterCamera(int cameraLogicalId)
{
    QMutexLocker locker(&m_cameraMutex);
    m_cameraMap.remove(cameraLogicalId);
}

CameraManager* Plugin::obtainCameraManager(const CameraInfo& cameraInfo, Error* outError)
{
    // We should invent more accurate test.
    if (cameraInfo.logicalId != 0)
        return new Manager(this, cameraInfo, m_typedManifest);
    else
        return nullptr;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::ssc::Plugin();
}

} // extern "C"

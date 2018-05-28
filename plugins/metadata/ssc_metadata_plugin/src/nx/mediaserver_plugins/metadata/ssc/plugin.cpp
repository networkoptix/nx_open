#include "plugin.h"

#include <sstream>
#include <iomanip>

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "manager.h"
#include "log.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

namespace {

static const char* const kPluginName = "SSC metadata plugin";

// The following constansts are from "SSC System Specification" (page 5.)
static const unsigned int kCommandLength = 4;
static const char kSTX = 0x02; //< start byte
static const char kETX = 0x03; //< stop byte
static const char kMinDigitCode = 0x30;
static const char kMinLogicalId = 1;
static const char kMaxLogicalId = 16;
static const char kResetCommand[] = {kSTX, 0x40, 0x40, kETX, '\0'};

QString normalize(const QString& name)
{
    QString result = name.toLower().simplified();
    result.replace(" ", "");
    return result;
}

void logReceivedCommand(const QByteArray& data)
{
    NX_ASSERT(data.size() >= kCommandLength);
    std::stringstream tempStream;
    for (int i = 0; i < kCommandLength; ++i)
        tempStream << std::hex << "0x" << std::setfill('0') << std::setw(2) << int(data[i]) << " ";
    NX_PRINT << "Bytes received via serial port: "<< tempStream.str();
}

int extractLogicalId(const QByteArray& data)
{
    return (data[1] - kMinDigitCode) * 10 + (data[2] - kMinDigitCode);
};
bool isCorrectCommand(const QByteArray& data)
{
    return data[0] == kSTX || data[3] == kETX;
};
bool isCorrectLogicalId(int cameraLogicalId)
{
    return cameraLogicalId >= kMinLogicalId && cameraLogicalId <= kMaxLogicalId;
};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin():m_serialPort(this)
{
    static const char* const kResourceName=":/ssc/manifest.json";
    static const char* const kFileName = "plugins/ssc/manifest.json";
    static const QString kCameraInternalName = "camera";
    static const QString kResetInternalName = "reset";

    QFile f(kResourceName);
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    {
        QFile file(kFileName);
        if (file.open(QFile::ReadOnly))
        {
            NX_PRINT << "Switch to external manifest file "
                << QFileInfo(file).absoluteFilePath().toStdString();
            m_manifest = file.readAll();
        }
    }
    f.close();

    m_typedManifest = QJson::deserialized<AnalyticsDriverManifest>(m_manifest);
    for (auto& model: m_typedManifest.supportedCameraModels)
        model = normalize(model);

    for (const auto& eventType: m_typedManifest.outputEventTypes)
    {
        if (eventType.internalName == kCameraInternalName)
            cameraEventType = eventType;
        else if (eventType.internalName == kResetInternalName)
            resetEventType = eventType;
    }

    tuneSerialPort();
}

void Plugin::tuneSerialPort()
{
    m_serialPort.setPortName(m_typedManifest.serialPortName);
    if (!m_serialPort.open(QIODevice::ReadOnly))
        NX_PRINT << "Serial port. Failed to open  " << m_typedManifest.serialPortName.toStdString();
    if (!m_serialPort.setBaudRate(QSerialPort::Baud9600, QSerialPort::Input))
        NX_PRINT << "Serial port. Failed to set 9600 baud";
    if (!m_serialPort.setDataBits(QSerialPort::Data8))
        NX_PRINT << "Serial port. Failed to set 8 data bits";
    if (!m_serialPort.setFlowControl(QSerialPort::NoFlowControl))
        NX_PRINT << "Serial port. Failed to set no flow control";
    if (!m_serialPort.setParity(QSerialPort::NoParity))
        NX_PRINT << "Serial port. Failed to set no parity";
    if (!m_serialPort.setStopBits(QSerialPort::OneStop))
        NX_PRINT << "Serial port. Failed to set 1 stop bit";

    QMetaObject::Connection connection =
        connect(&m_serialPort, &QSerialPort::readyRead, this, &Plugin::onSerialReceived);

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

void Plugin::onSerialReceived()
{
    static const int kResetId = extractLogicalId(QByteArray(kResetCommand));

    QByteArray data = m_serialPort.readAll();

    QMutexLocker locker(&m_managerMapMutex);

    while (data.size() >= kCommandLength)
    {
        logReceivedCommand(data);
        if(!isCorrectCommand(data))
            NX_PRINT << "bad command";
        int cameraLogicalId = extractLogicalId(data);
        if (cameraLogicalId == kResetId)
        {
            for (auto& manager: m_managerMap)
            {
                manager->sendEventPacket(resetEventType);
            }
        }
        else if (isCorrectLogicalId(cameraLogicalId))
        {
            auto it = m_managerMap.find(cameraLogicalId);
            if (it != m_managerMap.end())
                it.value()->sendEventPacket(cameraEventType);
            else
                NX_PRINT << "corresponding manager is not fetching metadata";
        }
        else
        {
            NX_PRINT << "incorrect data in command";
        }
        data.remove(0, kCommandLength);
    }
}

void Plugin::registerCamera(int cameraLogicalId, Manager* manager)
{
    QMutexLocker locker(&m_managerMapMutex);
    m_managerMap.insert(cameraLogicalId, manager);
}

void Plugin::unregisterCamera(int cameraLogicalId)
{
    QMutexLocker locker(&m_managerMapMutex);
    m_managerMap.remove(cameraLogicalId);
}

CameraManager* Plugin::obtainCameraManager(const CameraInfo& cameraInfo, Error* outError)
{
#if 0
    // This is for test purposes. Should be deleted when clien fill cameraInfo.logicalId field.
    if (true)
#else
    if (cameraInfo.logicalId != 0)
#endif
    {
        return new Manager(this, cameraInfo, m_typedManifest);
    }
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

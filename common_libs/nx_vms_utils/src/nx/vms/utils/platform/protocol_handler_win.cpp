#include "protocol_handler.h"

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <QtCore/QSettings>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace vms {
namespace utils {

using nx::utils::SoftwareVersion;

struct ProtocolHandlerFunctionsTag{};
static const nx::utils::log::Tag kLogTag(typeid(ProtocolHandlerFunctionsTag));

bool registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const SoftwareVersion& version)
{
    Q_UNUSED(applicationName)
    Q_UNUSED(customization)

    static const QString kClassesRootPath = "HKEY_CLASSES_ROOT\\";
    static const QString kDefaultValueKey = ".";
    static const QString kVersionKey = "version";

    QString protocolPath = kClassesRootPath + protocol;
    QString applicationNativePath = QDir::toNativeSeparators(applicationBinaryPath);

    QSettings registryEditor(protocolPath, QSettings::NativeFormat);
    SoftwareVersion existingVersion(registryEditor.value(kVersionKey).toString());
    if (existingVersion >= version)
        return true; /* Handler already registered. */

    registryEditor.setValue(kDefaultValueKey, description);
    registryEditor.setValue(kVersionKey, version.toString());
    registryEditor.setValue("URL Protocol", "");

    registryEditor.beginGroup("DefaultIcon");
    registryEditor.setValue(kDefaultValueKey, QString("%1 ,0").arg(applicationNativePath));
    registryEditor.endGroup();

    registryEditor.beginGroup("shell");
    registryEditor.setValue(kDefaultValueKey, "open");
    registryEditor.beginGroup("open");
    registryEditor.beginGroup("command");
    registryEditor.setValue(kDefaultValueKey, QString("\"%1\" -- \"%2\"").arg(applicationNativePath).arg("%1"));
    registryEditor.endGroup();
    registryEditor.endGroup();
    registryEditor.endGroup();
    registryEditor.sync();

    /* After sync() we get real value; empty if save was not successfull. */
    SoftwareVersion writtenVersion(registryEditor.value(kVersionKey).toString());
    return writtenVersion == version;
}

bool runAsAdministratorWithUAC(const QString& applicationBinaryPath, const QStringList& parameters)
{
    QString applicationNativePath = QDir::toNativeSeparators(applicationBinaryPath);

    auto applicationExecutable = applicationNativePath.toStdWString();
    auto paramString = parameters.join(' ').toStdWString();

    // This launches the application with the UAC prompt, and administrator rights are requested.
    HINSTANCE shellResult = ShellExecuteW(NULL, L"RUNAS", applicationExecutable.c_str(),
                                          paramString.c_str(), NULL, SW_SHOWNORMAL);

    int returnCode = static_cast<int>(reinterpret_cast<intptr_t>(shellResult));

    /*
     * ShellExecute returns a value less than 32 if it fails.
     * Usually in case of success it is 42 (Ultimate Answer).
     */
    static const int kMinimalSuccessCode = 32;
    if (returnCode >= kMinimalSuccessCode)
        return true;

    switch (returnCode)
    {
        case ERROR_ACCESS_DENIED:
            NX_INFO(kLogTag, "User denied to run executable as admin.");
            break;
        case ERROR_FILE_NOT_FOUND:
            NX_ERROR(kLogTag, "Could not find executable.");
            break;
        case ERROR_BAD_FORMAT:
            NX_ERROR(kLogTag, "Provided file isn't an executable.");
            break;
        default:
            NX_ERROR(kLogTag, "Unable to launch executable as admin.");
            break;
    }
    return false;
}


} // namespace utils
} // namespace vms
} // namespace nx

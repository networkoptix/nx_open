#include "protocol_handler.h"

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <QtCore/QSettings>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>

namespace nx
{
    namespace utils
    {
        bool registerSystemUriProtocolHandler(const QString& protocol, const QString& applicationBinaryPath, const QString& description)
        {
            static const QString kClassesRootPath = "HKEY_CLASSES_ROOT\\";
            static const QString kDefaultValueKey = ".";

            QString protocolPath = kClassesRootPath + protocol;
            QString applicationNativePath = QDir::toNativeSeparators(applicationBinaryPath);


            QSettings registryEditor(protocolPath, QSettings::NativeFormat);
            if (!registryEditor.value(kDefaultValueKey).isNull())
                return true; /* Handler already registered. */

            registryEditor.setValue(kDefaultValueKey, QString("URL:%1").arg(description));
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
            return !registryEditor.value(kDefaultValueKey).isNull();
        }

        bool runAsAdministratorWithUAC(const QString &applicationBinaryPath, const QStringList &parameters)
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
                    NX_LOG("User denied to run executable as admin.", cl_logINFO);
                    break;
                case ERROR_FILE_NOT_FOUND:
                    NX_LOG("Could not find executable.", cl_logERROR);
                    break;
                case ERROR_BAD_FORMAT:
                    NX_LOG("Provided file isn't an executable.", cl_logERROR);
                    break;
                default:
                    NX_LOG("Unable to launch executable as admin.", cl_logERROR);
                    break;
            }
            return false;
        }


    }
}

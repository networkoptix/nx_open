#include "protocol_handler.h"

#include <QtCore/QSettings>
#include <QtCore/QDir>

namespace nx
{
    namespace utils
    {
        bool registerSystemUriProtocolHandler(const QString& protocol, const QString& applicationBinaryPath)
        {
            static const QString kClassesRootPath = "HKEY_CLASSES_ROOT\\";
            static const QString kDefaultValueKey = ".";

            QString protocolPath = kClassesRootPath + protocol;
            QString applicationNativePath = QDir::toNativeSeparators(applicationBinaryPath);

            QSettings registryEditor(protocolPath, QSettings::NativeFormat);
            registryEditor.setValue(kDefaultValueKey, "URL:NxWitness Client Protocol");
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

            return true;
        }
    }
}

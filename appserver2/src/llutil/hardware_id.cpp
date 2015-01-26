#include <QtGlobal>
#include <QDebug>
#include <QtCore/QCryptographicHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSettings>
#include <QtCore/QMutex>

#include "hardware_id.h"

namespace {

QList<QByteArray> g_hardwareId;
bool g_hardwareIdInitialized(false);
QMutex g_hardwareIdMutex;

}

namespace LLUtil {

void fillHardwareIds(QList<QByteArray>& hardwareIds);

QByteArray getHardwareId(int version, bool guidCompatibility) {
    QMutexLocker _lock(&g_hardwareIdMutex);

    if (version == 0) {
        QByteArray hardwareId = QSettings().value("ecsGuid").toString().toLatin1();
        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
        md5Hash.addData(hardwareId);
        return QString(md5Hash.result().toHex()).toLatin1();
    }
    
    if (!g_hardwareIdInitialized) {
        // Initialize with 2 * LATEST_HWID_VERSION elements
        for (int i = 0; i < 2 * LATEST_HWID_VERSION; i++) {
            g_hardwareId << QByteArray();
        }
        try {
            fillHardwareIds(g_hardwareId);
            g_hardwareIdInitialized = true;
        } catch (const LLUtil::HardwareIdError& err) {
            qWarning() << QLatin1String("getHardwareId()") << err.what();
            return QByteArray();
        }
    }

    int index = guidCompatibility ? (version - 1 + LATEST_HWID_VERSION) : (version - 1);

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(g_hardwareId[index]);
    return QString(lit("0%1%2")).arg(version).arg(QString(md5Hash.result().toHex())).toLatin1();
}

QList<QByteArray> getMainHardwareIds(int guidCompatibility) {
    QList<QByteArray> hardwareIds;

    for (int i = 0; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility == 1));
    }

    return hardwareIds;
}

QList<QByteArray> getCompatibleHardwareIds(int guidCompatibility) {
    QList<QByteArray> hardwareIds;

    for (int i = 1; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility != 1));
    }

    return hardwareIds;
}

} // namespace LLUtil {}

#include <QtGlobal>
#include <QDebug>
#include <QtCore/QCryptographicHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSettings>

#include "hardware_id.h"

namespace {

    QList<QByteArray> g_hardwareId;
    bool g_hardwareIdInitialized(false);
}

namespace LLUtil {
    void fillHardwareIds(QList<QByteArray>& hardwareIds, QSettings *settings);

    QString getSaveMacAddress(std::vector<DeviceClassAndMac> devices, QSettings *settings) {
        if (devices.empty())
            return QString();

        std::sort(devices.begin(), devices.end(), [](const DeviceClassAndMac &device1, const DeviceClassAndMac &device2) {
                if (device1.xclass < device2.xclass)
                    return true;
                else if (device1.xclass > device2.xclass)
                    return false;
                else
                    return device1.mac < device2.mac;
            });

        QString storedMac = settings->value("storedMac").toString();

        QString result;

        for(auto it = devices.begin(); it != devices.end(); ++it) {
            if  (it->mac == storedMac) {
                result = storedMac;
                break;
            }
        }

        if (result.isEmpty()) {
            result = devices.front().mac;
            settings->setValue("storedMac", result);
        }

        return result;
    }


QByteArray getHardwareId(int version, bool guidCompatibility, QSettings *settings) {
    if (version == 0) {
        QByteArray hardwareId = QSettings().value("ecsGuid").toString().toLatin1();
        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
        md5Hash.addData(hardwareId);
        return QString(md5Hash.result().toHex()).toLatin1();
    }

    if (!g_hardwareIdInitialized) {
        // Initialize with LATEST_HWID_VERSION * 2 elements
        for (int i = 0; i < 2 * LATEST_HWID_VERSION; i++) {
            g_hardwareId << QByteArray();
        }
        try {
            fillHardwareIds(g_hardwareId, settings);
            g_hardwareIdInitialized = true;
        } catch (const LLUtil::HardwareIdError& err) {
            qWarning() << QLatin1String("getHardwareId()") << err.what();
            return QByteArray();
        }
    }

    if (version < 0 || version > LATEST_HWID_VERSION) {
        qWarning() << QLatin1String("getHardwareId(): requested hwid of invalid version: ") << version;
        return QByteArray();
    }

    int index = guidCompatibility ? (version - 1 + LATEST_HWID_VERSION) : (version - 1);

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(g_hardwareId[index]);
    return QString(lit("0%1%2")).arg(version).arg(QString(md5Hash.result().toHex())).toLatin1();
}

QList<QByteArray> getMainHardwareIds(int guidCompatibility, QSettings *settings) {
    QList<QByteArray> hardwareIds;

    for (int i = 0; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility == 1, settings));
    }

    return hardwareIds;
}

QList<QByteArray> getCompatibleHardwareIds(int guidCompatibility, QSettings *settings) {
    QList<QByteArray> hardwareIds;

    for (int i = 1; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility != 1, settings));
    }

    return hardwareIds;
}

} // namespace LLUtil {}

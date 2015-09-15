#include <QtGlobal>
#include <QDebug>
#include <QtCore/QCryptographicHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>

#include <utils/serialization/json.h>

#include "hardware_id.h"
#include "hardware_id_p.h"

#include "utils/common/log.h"
#include "licensing/hardware_info.h"

namespace {
    QnHardwareInfo g_hardwareInfo;
    QStringList g_hardwareId;
    bool g_hardwareIdInitialized(false);
}

namespace LLUtil {

    void fillHardwareIds(QStringList& hardwareIds, QSettings *settings, QnHardwareInfo& hardwareInfo);

    QString getSaveMacAddress(const QnMacAndDeviceClassList& devices, QSettings *settings) {
        if (devices.empty())
            return QString();

        QnMacAndDeviceClassList devicesCopy(devices);
        std::sort(devicesCopy.begin(), devicesCopy.end(), [](const QnMacAndDeviceClass &device1, const QnMacAndDeviceClass &device2) {
                if (device1.xclass < device2.xclass)
                    return true;
                else if (device1.xclass > device2.xclass)
                    return false;
                else
                    return device1.mac < device2.mac;
            });

        QString storedMac = settings->value("storedMac").toString();

        QString result;

        for(auto it = devicesCopy.begin(); it != devicesCopy.end(); ++it) {
            if  (it->mac == storedMac) {
                result = storedMac;
                break;
            }
        }

        if (result.isEmpty()) {
            result = devicesCopy.front().mac;
            settings->setValue("storedMac", result);
        }

        return result;
    }


QString getHardwareId(int version, bool guidCompatibility, QSettings *settings) {
    if (version == 0) {
        QString hardwareId = QSettings().value("ecsGuid").toString();
        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
        md5Hash.addData(hardwareId.toUtf8());
        return QString(md5Hash.result().toHex()).toLatin1();
    }

    if (!g_hardwareIdInitialized) {
        // Initialize with LATEST_HWID_VERSION * 2 elements
        for (int i = 0; i < 2 * LATEST_HWID_VERSION; i++) {
            g_hardwareId << QByteArray();
        }

        try {
            fillHardwareIds(g_hardwareId, settings, g_hardwareInfo);
            Q_ASSERT(g_hardwareId.size() == 2 * LATEST_HWID_VERSION);

            g_hardwareInfo.date = QDateTime::currentDateTime().toString();
            NX_LOG(QnLog::HWID_LOG, QString::fromUtf8(QJson::serialized(g_hardwareInfo)).trimmed(), cl_logINFO);

            g_hardwareIdInitialized = true;
        } catch (const LLUtil::HardwareIdError& err) {
            qWarning() << QLatin1String("getHardwareId()") << err.what();
            return QString();
        }
    }

    if (version < 0 || version > LATEST_HWID_VERSION) {
        qWarning() << QLatin1String("getHardwareId(): requested hwid of invalid version: ") << version;
        return QString();
    }

    int index = guidCompatibility ? (version - 1 + LATEST_HWID_VERSION) : (version - 1);

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(g_hardwareId[index].toUtf8());
    return QString(lit("0%1%2")).arg(version).arg(QString(md5Hash.result().toHex()));
}

QStringList getMainHardwareIds(int guidCompatibility, QSettings *settings) {
    QStringList hardwareIds;

    for (int i = 0; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility == 1, settings));
    }

    return hardwareIds;
}

QStringList getCompatibleHardwareIds(int guidCompatibility, QSettings *settings) {
    QStringList hardwareIds;

    for (int i = 1; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds.append(getHardwareId(i, guidCompatibility != 1, settings));
    }

    return hardwareIds;
}

const QnHardwareInfo& getHardwareInfo() {
    return g_hardwareInfo;
}
} // namespace LLUtil {}

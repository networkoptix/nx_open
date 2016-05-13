#include <QtGlobal>
#include <QDebug>
#include <QtCore/QCryptographicHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <array>

#include <utils/serialization/json.h>

#include "hardware_id.h"
#include "hardware_id_p.h"

#include <nx/utils/log/log.h>
#include "licensing/hardware_info.h"

namespace {
    QnHardwareInfo g_hardwareInfo;
    HardwareIdListType g_hardwareId;
    QString g_storedMac;
    bool g_hardwareIdInitialized(false);
}

namespace LLUtil {

    void fillHardwareIds(HardwareIdListType& hardwareIds, QnHardwareInfo& hardwareInfo);

    void calcHardwareIds(HardwareIdListForVersion& macHardwareIds, const QnHardwareInfo& hardwareInfo, int version) {
        macHardwareIds.clear();

        std::array<bool, 2> guidCompatibilities = { false, true };

        QStringList macs = version >= 4 ? getMacAddressList(hardwareInfo.nics) : QStringList("");
        for (QString mac : macs) {
            QStringList hardwareIds;
            for (bool guidCompatibility : guidCompatibilities) {
                QMap<QString, QString> hardwareIdMap;
                calcHardwareIdMap(hardwareIdMap, hardwareInfo, version, guidCompatibility);
                hardwareIds << hardwareIdMap[mac];
            }

            macHardwareIds << QPair<QString, QStringList>(mac, hardwareIds);
        }

        for (auto& item : macHardwareIds) {
            item.second.removeDuplicates();
        }
    }


    QStringList getMacAddressList(const QnMacAndDeviceClassList& devices) {
        if (devices.empty())
            return QStringList();

        QnMacAndDeviceClassList devicesCopy(devices);
        std::sort(devicesCopy.begin(), devicesCopy.end(), [](const QnMacAndDeviceClass &device1, const QnMacAndDeviceClass &device2) {
            if (device1.xclass < device2.xclass)
                return true;
            else if (device1.xclass > device2.xclass)
                return false;
            else
                return device1.mac < device2.mac;
        });

        QStringList result;

        for (const QnMacAndDeviceClass& device: devicesCopy) {
            result.append(device.mac);
        }

        return result;
    }

    QString saveMac(const QStringList& macs, QSettings *settings) {
        if (macs.isEmpty())
            return QString();

        QString storedMac = settings->value("storedMac").toString();

        if (!macs.contains(storedMac)) {
            storedMac = macs.front();
            settings->setValue("storedMac", storedMac);
        }

        return storedMac;
    }

    void initHardwareId(QSettings *settings) {
        if (g_hardwareIdInitialized)
            return;

        try {
            fillHardwareIds(g_hardwareId, g_hardwareInfo);
            NX_ASSERT(g_hardwareId.size() == LATEST_HWID_VERSION);

            g_hardwareInfo.date = QDateTime::currentDateTime().toString(Qt::ISODate);
            QStringList macs = getMacAddressList(g_hardwareInfo.nics);
            g_storedMac = saveMac(macs, settings);
            g_hardwareInfo.mac = g_storedMac;

            g_hardwareIdInitialized = true;

            NX_LOG(QnLog::HWID_LOG, QString::fromUtf8(QJson::serialized(g_hardwareInfo)).trimmed(), cl_logINFO);
            NX_LOG(QnLog::HWID_LOG, QString("Hardware IDs: [\"%1\"]").arg(getAllHardwareIds().join("\", \"")), cl_logINFO);
        }
        catch (const LLUtil::HardwareIdError& err) {
            qWarning() << QLatin1String("getHardwareId()") << err.what();
        }
    }

    /* Hardware IDs come in order:
        If it depends on a MAC, then saved mac first.
        First come ones with guid_compatibility=0 */
    QStringList getHardwareIds(int version) {
        if (version == 0) {
            QString hardwareId = QSettings().value("ecsGuid").toString();
            QCryptographicHash md5Hash(QCryptographicHash::Md5);
            md5Hash.addData(hardwareId.toUtf8());
            return QStringList() << QString(md5Hash.result().toHex()).toLatin1();
        }

        if (version < 0 || version > LATEST_HWID_VERSION) {
            qWarning() << QLatin1String("getHardwareId(): requested hwid of invalid version: ") << version;
            return QStringList();
        }

        int index = version - 1;

        QStringList result;
        for (const auto& pair : g_hardwareId[index]) {
            for (const QString& hwid : pair.second) {
                QCryptographicHash md5Hash(QCryptographicHash::Md5);
                md5Hash.addData(hwid.toUtf8());

                QString hwid = QString(lit("0%1%2")).arg(version).arg(QString(md5Hash.result().toHex()));
                if (pair.first == g_storedMac)
                    result.insert(0, hwid);
                else
                    result << hwid;
            }
        }

        return result;
    }

    QStringList getAllHardwareIds() {
        NX_ASSERT(g_hardwareIdInitialized);

        QStringList hardwareIds;

        for (int i = 0; i <= LATEST_HWID_VERSION; i++) {
            hardwareIds.append(getHardwareIds(i));
        }

        return hardwareIds;
    }

    QString getLatestHardwareId() {
        NX_ASSERT(g_hardwareIdInitialized);

        const auto& macHwIds = g_hardwareId[LATEST_HWID_VERSION - 1];
        for (const auto& macHwid : macHwIds) {
            if (macHwid.first == g_storedMac) {
                if (!macHwid.second.isEmpty()) {
                    QCryptographicHash md5Hash(QCryptographicHash::Md5);
                    md5Hash.addData(macHwid.second.front().toUtf8());
                    return QString(lit("0%1%2")).arg(LATEST_HWID_VERSION).arg(QString(md5Hash.result().toHex()));
                }
            }
        }

        return QString();
    }

    const QnHardwareInfo& getHardwareInfo() {
        return g_hardwareInfo;
    }

    int hardwareIdVersion(const QString& hardwareId) {
        if (hardwareId.length() == 32)
            return 0;
        else if (hardwareId.length() == 34) {
            return hardwareId.mid(0, 2).toInt();
        }

        return -1;
    }
} // namespace LLUtil {}

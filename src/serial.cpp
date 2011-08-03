#include "serial.h"

#include <QtCore/QCryptographicHash>

SerialChecker::SerialChecker()
{
#include "serials.ipp"
}

bool SerialChecker::isValidSerial(const QString& serial)
{
    QString serialHash = QString::fromAscii(QCryptographicHash::hash(serial.toAscii(), QCryptographicHash::Md5).toHex().constData());
    return m_serialHashes.contains(serialHash);
}

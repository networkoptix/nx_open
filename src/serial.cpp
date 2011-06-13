#include "serial.h"

SerialChecker::SerialChecker()
{
#include "serials.ipp"
}

bool SerialChecker::isValidSerial(const QString& serial)
{
    QString serialHash(QCryptographicHash::hash(serial.toAscii(), QCryptographicHash::Md5).toHex());
    return m_serialHashes.indexOf(serialHash) != -1;
}

#ifndef _uniclient_serial_h
#define _uniclient_serial_h

class SerialChecker
{
public:
    SerialChecker();
    bool isValidSerial(const QString& serial);

private:
    QStringList m_serialHashes;
};

#endif // _uniclient_serial_h
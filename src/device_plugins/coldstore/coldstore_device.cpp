#include "coldstore_device.h"
#include "coldstore_strem_reader.h"


ColdStoreDevice::ColdStoreDevice(const QString& id, const QString& name, const QString& file, QHostAddress addr)
{
    m_uniqueId = id;
    setName(name);
    m_filename = file;
    m_cs_addr = addr;
}

ColdStoreDevice::~ColdStoreDevice()
{

}

QString ColdStoreDevice::toString() const
{
    return m_name;
}

CLStreamreader* ColdStoreDevice::getDeviceStreamConnection()
{
    return new ColdStoreStreamReader(this);
}

const CLDeviceVideoLayout* ColdStoreDevice::getVideoLayout(CLStreamreader* reader)
{
    return CLDevice::getVideoLayout(reader);
}

QHostAddress ColdStoreDevice::getCSAddr() const
{
    return m_cs_addr;    
}

void ColdStoreDevice::setCSAddr(const QHostAddress& addr)
{
    m_cs_addr = addr;
}

QString ColdStoreDevice::getFileName() const
{
    return m_filename;
}
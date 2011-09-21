#ifndef cold_store_resource_h_1829
#define cold_store_resource_h_1829

#include "../archive/abstract_archive_device.h"




class ColdStoreDevice : public CLAbstractArchiveDevice
{
public:
    ColdStoreDevice(const QString& id, const QString& name, const QString& file, QHostAddress addr);
    ~ColdStoreDevice();

    virtual CLStreamreader* getDeviceStreamConnection();
    virtual QString toString() const;
    virtual const CLDeviceVideoLayout* getVideoLayout(CLStreamreader* reader);

    QHostAddress getCSAddr() const;
    void setCSAddr(const QHostAddress& addr);

    QString getFileName() const;
private:

    QHostAddress m_cs_addr;
    QString m_filename;
};




#endif //cold_store_resource_h_1829
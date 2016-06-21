#pragma once

#include <core/datapacket/abstract_data_packet.h>

class QnAbstractDataSource
{
public:
    
    virtual QnAbstractDataPacketPtr retrieveData() = 0;
};

typedef std::shared_ptr<QnAbstractDataSource> QnAbstractDataSourcePtr;
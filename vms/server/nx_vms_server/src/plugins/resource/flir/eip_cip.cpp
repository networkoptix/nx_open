#include "eip_cip.h"

#include <QtCore/QDataStream>

QByteArray MessageRouterRequest::encode(const MessageRouterRequest& request)
{
    QByteArray encoded;
    encoded[0] = request.serviceCode;
    encoded[1] = request.pathSize;
    encoded.append(request.epath);
    encoded.append(request.data);

    return encoded;
}

QByteArray MessageRouterRequest::buildEPath(quint8 classId, quint8 instanceId, quint8 attributeId)
{
    QByteArray epath;
    epath[0] = CIPCommonLogicalSegment::kClassSegment8;
    epath[1] = classId;
    epath[2] = CIPCommonLogicalSegment::kInstanceSegment8;
    epath[3] = instanceId;
    if(attributeId != 0)
    {
        epath[4] = CIPCommonLogicalSegment::kAttributeSegment8;
        epath[5] = attributeId;
    }

    return epath;
}

MessageRouterResponse MessageRouterResponse::decode(const QByteArray& buf)
{
    MessageRouterResponse response;
    QDataStream stream(const_cast<QByteArray*>(&buf), QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream
        >> response.serviceCode
        >> response.reserved
        >> response.generalStatus
        >> response.sizeOfAdditionalStatus;

    if(response.sizeOfAdditionalStatus > 0)
    {
        std::unique_ptr<char[]> tmp(new char[response.sizeOfAdditionalStatus]);
        stream.readRawData(tmp.get(), response.sizeOfAdditionalStatus);
        response.additionalStatus.append(tmp.get(), response.sizeOfAdditionalStatus);
    }

    int sizeOfData = buf.size()
        - sizeof(response.serviceCode)
        - sizeof(response.reserved)
        - sizeof(response.generalStatus)
        - sizeof(response.sizeOfAdditionalStatus)
        - response.sizeOfAdditionalStatus;

    if(sizeOfData > 0)
    {
        std::unique_ptr<char[]> tmp(new char[sizeOfData]);
        stream.readRawData(tmp.get(), sizeOfData);
        response.data.append(tmp.get(), sizeOfData);
    }


    return response;
}

QByteArray CPFPacket::encode(const CPFPacket& packet)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << packet.itemCount;
    for(const auto& item: packet.items)
        encoded.append(CPFItem::encode(item));

    return encoded;
}

CPFPacket CPFPacket::decode(const QByteArray& buf)
{
    CPFPacket packet;
    QDataStream stream(const_cast<QByteArray*>(&buf), QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> packet.itemCount;
    for(size_t i = 0; i < packet.itemCount; i++)
    {
        CPFItem cpfItem;
        stream
            >> cpfItem.typeId
            >> cpfItem.dataLength;

        std::unique_ptr<char[]> tmp(new char[cpfItem.dataLength]);
        stream.readRawData(tmp.get(), cpfItem.dataLength);\

        cpfItem.data.append(tmp.get(), cpfItem.dataLength);
        packet.items.push_back(cpfItem);
    }

    return packet;
}

QByteArray CPFItem::encode(const CPFItem& item)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << item.typeId << item.dataLength;
    if(item.typeId != CIPItemID::kNullAddress)
        encoded.append(item.data);

    return encoded;
}

QByteArray EIPEncapsulationHeader::encode(const EIPEncapsulationHeader& encHeader)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream
        << encHeader.commandCode
        << encHeader.dataLength
        << encHeader.sessionHandle
        << encHeader.status
        << encHeader.senderContext
        << encHeader.options;

    return encoded;
}

QByteArray EIPEncapsulationData::encode(const EIPEncapsulationData& encData)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << encData.handle << encData.timeout;
    encoded.append(CPFPacket::encode(encData.cpfPacket));

    return encoded;
}

QByteArray EIPPacket::encode(const EIPPacket& packet)
{
    QByteArray encoded;
    auto data = EIPEncapsulationData::encode(packet.data);
    auto header = packet.header;

    header.dataLength = data.size();

    encoded.append(EIPEncapsulationHeader::encode(header));
    encoded.append(data);

    return encoded;
}

EIPEncapsulationHeader EIPPacket::parseHeader(const QByteArray& buf)
{
    EIPEncapsulationHeader encHeader;
    auto headerBuf = buf.left(EIPEncapsulationHeader::SIZE);

    QDataStream input(&headerBuf, QIODevice::ReadOnly);
    input.setByteOrder(QDataStream::LittleEndian);

    input
        >> encHeader.commandCode
        >> encHeader.dataLength
        >> encHeader.sessionHandle
        >> encHeader.status
        >> encHeader.senderContext
        >> encHeader.options;

    return encHeader;
}


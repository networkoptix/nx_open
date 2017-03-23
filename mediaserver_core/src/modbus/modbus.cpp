#include "modbus.h"
#include <QtCore/QIODevice>
#include <QtCore/QDataStream>

namespace nx
{
namespace modbus
{

QString exceptionDescriptionByCode(quint8 exceptionCode)
{
    switch (exceptionCode)
    {
        case ExceptionFunctionCode::kIllegalFunction:
            return lit("Illegal request function code");
        case ExceptionFunctionCode::kIllegalDataAddress:
            return lit("Illegal data address");
        case ExceptionFunctionCode::kIllegalDataValue:
            return lit("Illegal data value");
        case ExceptionFunctionCode::kServerDeviceFailure:
            return lit("Server device failure");
        case ExceptionFunctionCode::kAcknowledge:
            return lit("Acknowledge. Operation in progress. Not an error.");
        case ExceptionFunctionCode::kServerDeviceBusy:
            return lit("Server device is busy");
        case ExceptionFunctionCode::kMemoryParityError:
            return lit("Memory parity error");
        case ExceptionFunctionCode::kGatewayPathUnavailable:
            return lit("Gateway path unavailable");
        case ExceptionFunctionCode::kGatewayTargetDeviceFailedToRespond:
            return lit("Gateway target device failed to respond");
        default:
            return lit("Unknown error");
    }
}

ModbusMBAPHeader::ModbusMBAPHeader():
    transactionId(0),
    protocolId(0),
    length(sizeof(decltype(ModbusMBAPHeader::unitId))),
    unitId(0)
{
}

QByteArray ModbusMBAPHeader::encode(const ModbusMBAPHeader& header)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream
        << header.transactionId
        << header.protocolId
        << header.length
        << header.unitId;

    return encoded;
}

ModbusMBAPHeader ModbusMBAPHeader::decode(const QByteArray& header)
{
    ModbusMBAPHeader decoded;

    QDataStream stream(const_cast<QByteArray*>(&header), QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream
        >> decoded.transactionId
        >> decoded.protocolId
        >> decoded.length
        >> decoded.unitId;

    return decoded;
}

ModbusMessage::ModbusMessage():
    header(ModbusMBAPHeader()),
    functionCode(0)
{
}

QByteArray ModbusRequest::encode(const ModbusRequest &request)
{
    QByteArray encoded;

    encoded.append(ModbusMBAPHeader::encode(request.header));
    encoded.push_back(request.functionCode);
    encoded.append(request.data);

    return encoded;
}

ModbusResponse ModbusResponse::decode(const QByteArray &response)
{
    ModbusResponse decoded;

    auto header = ModbusMBAPHeader::decode(
        response.left(ModbusMBAPHeader::size));

    auto headerless = response.mid(ModbusMBAPHeader::size);

    decoded.exceptionFunctionCode = 0;
    decoded.header = header;
    decoded.functionCode = headerless[0];

    if(decoded.functionCode > kErrorCodeLowerLimit)
    {
        decoded.exceptionFunctionCode = headerless[1];
        return decoded;
    }

    decoded.data = headerless.mid(1);
    return decoded;
}

bool ModbusResponse::isException() const
{
    return functionCode > kErrorCodeLowerLimit;
}

QString ModbusResponse::getExceptionString() const
{
    if (functionCode < kErrorCodeLowerLimit)
        return lit("Everything is fine.");

    return exceptionDescriptionByCode(exceptionFunctionCode);
}

QByteArray ModbusIdRequestData::encode(const ModbusIdRequestData& /*request*/)
{
    NX_ASSERT(false, "ModbusIdRequestData::encode is not implemented");
    return QByteArray();
}

ModbusIdResponseData ModbusIdResponseData::decode(const QByteArray& /*response*/)
{
    NX_ASSERT(false, "ModbusIdResponseData::decode is not implemented");
    return ModbusIdResponseData();
}

bool ModbusMessage::isException() const
{
    return functionCode > kErrorCodeLowerLimit;
}

QString ModbusMessage::getExceptionString() const
{
    if (!isException())
        return lit("Everything is fine.");

    if (data.isEmpty())
        return lit("Invalid message, no exception code.");

    quint8 exceptionCode = data[0];

    return exceptionDescriptionByCode(exceptionCode);
}

nx::Buffer ModbusMessage::encode() const
{
    nx::Buffer encoded;

    encoded.append(ModbusMBAPHeader::encode(header));
    encoded.push_back(functionCode);
    encoded.append(data);

    return encoded;
}

void ModbusMessage::decode(const nx::Buffer& buffer)
{
    header = ModbusMBAPHeader::decode(
        buffer.left(ModbusMBAPHeader::size));

    auto headerless = buffer.mid(ModbusMBAPHeader::size);
    functionCode = headerless[0];
    data = headerless.mid(1);
}

void ModbusMessage::clear()
{
    header = ModbusMBAPHeader();
    functionCode = 0;
    data.clear();
}

} //< Closing namespace modbus

} //< Closing namespace nx

#pragma once

#include <QtCore/QList>

#include <nx/network/buffer.h>

namespace nx
{
namespace modbus
{

const quint16 kDefaultModbusPort = 502;
const quint8 kErrorCodeLowerLimit = 0x80;
const int kModbusMaxMessageLength = 512;

const quint16 kCoilStateOn = 0xff00;
const quint16 kCoilStateOff = 0x0000;

namespace FunctionCode {
    const quint8 kReadCoils = 0x01;
    const quint8 kReadDiscreteInputs = 0x02;
    const quint8 kReadHoldingRegisters = 0x03;
    const quint8 kReadInputRegisters = 0x04;
    const quint8 kWriteSingleCoil = 0x05;
    const quint8 kWriteSingleRegister = 0x06;
    const quint8 kWriteMultipleCoils = 0x0f;
    const quint8 kWriteMultipleRegisters = 0x10;
    const quint8 kReadFileRecord = 0x14;
    const quint8 kWriteFileRecord = 0x15;
    const quint8 kMaskWriteRegister = 0x16;
    const quint8 kReadWriteMultipleRegisters = 0x17;
    const quint8 kReadFIFOQueue = 0x18;
    const quint8 kEncapsulatedInterfaceTransport = 0x2b;
}

namespace ExceptionFunctionCode
{
    const quint8 kIllegalFunction = 0x01;
    const quint8 kIllegalDataAddress = 0x02;
    const quint8 kIllegalDataValue = 0x03;
    const quint8 kServerDeviceFailure = 0x04;
    const quint8 kAcknowledge = 0x05;
    const quint8 kServerDeviceBusy = 0x06;
    const quint8 kMemoryParityError = 0x08;
    const quint8 kGatewayPathUnavailable = 0x0a;
    const quint8 kGatewayTargetDeviceFailedToRespond = 0x0b;
}

namespace MEIType
{
    const quint8 kCANOpenGeneralReference = 0x0d;
    const quint8 kReadDeviceIdentification = 0x0e;
}

namespace DeviceIdCode
{
    const quint8 kBasicDeviceId = 0x01;
    const quint8 kRegularDeviceId = 0x02;
    const quint8 kExtendedDeviceId = 0x03;
    const quint8 kSpecificDeviceId = 0x04;
}

struct ModbusMBAPHeader
{
    quint16 transactionId;
    quint16 protocolId;
    quint16 length;
    quint8 unitId;

    ModbusMBAPHeader();

    static QByteArray encode(const ModbusMBAPHeader& header);
    static ModbusMBAPHeader decode(const QByteArray& header);
    static const size_t size =
        sizeof(decltype(ModbusMBAPHeader::transactionId)) +
        sizeof(decltype(ModbusMBAPHeader::protocolId)) +
        sizeof(decltype(ModbusMBAPHeader::length)) +
        sizeof(decltype(ModbusMBAPHeader::unitId));
};

QString exceptionDescriptionByCode(quint8 code);

struct ModbusMessage
{
    ModbusMBAPHeader header;
    quint8 functionCode;
    QByteArray data;

    ModbusMessage();

    bool isException() const;
    QString getExceptionString() const;
    nx::Buffer encode() const;
    void decode(const nx::Buffer& buffer);

    void clear();

    static const quint8 kHeaderSize = ModbusMBAPHeader::size
        + sizeof(decltype(functionCode));
};

struct ModbusRequest
{
    ModbusMBAPHeader header;
    quint8 functionCode;
    QByteArray data;

    static QByteArray encode(const ModbusRequest& request);
};

struct ModbusResponse
{
    ModbusMBAPHeader header;
    quint8 functionCode;
    quint8 exceptionFunctionCode;
    QByteArray data;

    bool isException() const;
    QString getExceptionString() const;

    static ModbusResponse decode(const QByteArray& response);
};

/*Id request is not supported now*/
struct ModbusIdRequestData
{
    quint8 MEIType;
    quint8 deviceIdCode;
    quint8 objectId;

    static QByteArray encode(const ModbusIdRequestData& request);
};

struct ModbusIdRecord
{
    quint8 objectId;
    quint8 objectLength;
    QByteArray objectValue;
};

struct ModbusIdResponseData
{
    quint8 MEIType;
    quint8 deviceIdCode;
    quint8 conformityLevel;
    quint8 moreFollows;
    quint8 nextObjectId;
    quint8 numberOfObjects;
    QList<ModbusIdRecord> values;

    static ModbusIdResponseData decode(const QByteArray& response);
};

} //< Closing namespace modbus.

} //< Closing namespace nx.

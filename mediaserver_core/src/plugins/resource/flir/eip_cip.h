#ifndef EIP_CIP_H
#define EIP_CIP_H

#include <QHostAddress>
#include <utils/network/system_socket.h>
#include <utils/network/socket.h>
#include <cstdint>

typedef QSharedPointer<AbstractStreamSocket> TCPSocketPtr;
typedef quint16 eip_status_t;
typedef quint8 cip_general_status_t;
typedef quint32 eip_session_handle_t;

struct EIPEncapsulationHeader
{
    quint16 commandCode;
    quint16 dataLength;
    quint32 sessionHandle;
    quint32 status;
    quint64 senderContext;
    quint32 options;

    static const size_t SIZE = 192;

    static QByteArray encode(const EIPEncapsulationHeader& header);
};

struct CPFItem
{
    quint16 typeId;
    quint16 dataLength;
    QByteArray data;

    static QByteArray encode(const CPFItem&);
};

struct CPFPacket
{
    quint16 itemCount;
    QList<CPFItem> items;
    static QByteArray encode(const CPFPacket&);
    static CPFPacket decode(const QByteArray&);
};

struct EIPEncapsulationData
{
    quint32 handle;
    quint16 timeout;
    CPFPacket cpfPacket;

    static QByteArray encode(const EIPEncapsulationData& data);
};

struct MessageRouterRequest
{
    quint8 serviceCode;
    quint8 pathSize;
    QByteArray epath;
    QByteArray data;

    static QByteArray encode(const MessageRouterRequest& request);
    static QByteArray buildEPath(quint8 classId, quint8 instanceId, quint8 attributeId = 0);
};

struct MessageRouterResponse
{
    quint8 serviceCode;
    quint8 reserved;
    quint8 generalStatus;
    quint8 sizeOfAdditionalStatus;
    QByteArray additionalStatus;
    QByteArray data;

    static QByteArray encode(const MessageRouterRequest& request);
    static MessageRouterResponse decode(const QByteArray& buf);
};

struct EIPPacket
{
    EIPEncapsulationHeader header;
    EIPEncapsulationData data;

    static QByteArray encode(const EIPPacket& packet);
    static EIPPacket decode(const QByteArray& buf);
    static EIPEncapsulationHeader parseHeader(const QByteArray& buf);
    static EIPEncapsulationData parseData(const QByteArray& buf);
};

struct CIPPath
{
    CIPPath(): classId(0), instanceId(0), attributeId(0){}
    quint8 classId;
    quint8 instanceId;
    quint8 attributeId;
};

namespace EIPCommand
{
    const quint16 EIP_COMMAND_NOP = 0x0000;
    const quint16 EIP_COMMAND_LIST_SERVICES = 0x0004;
    const quint16 EIP_COMMAND_LIST_IDENTITY = 0x0063;
    const quint16 EIP_COMMAND_LIST_INTERFACES = 0x0064;
    const quint16 EIP_COMMAND_REGISTER_SESSION = 0x0065;
    const quint16 EIP_COMMAND_UNREGISTER_SESSION = 0x0066;
    const quint16 EIP_COMMAND_SEND_RR_DATA = 0x006F;
    const quint16 EIP_COMMAND_SEND_UNIT_DATA = 0x0070;
    const quint16 EIP_COMMAND_INDICATE_STATUS = 0x0072;
    const quint16 EIP_COMMAND_CANCEL = 0x0073;
}

namespace EIPStatus
{
    const quint16 EIP_STATUS_SUCCESS = 0x0000;
    const quint16 EIP_STATUS_INVALID_COMMAND = 0x0001;
    const quint16 EIP_STATUS_INSUFFICIENT_MEMORY = 0x0002;
    const quint16 EIP_STATUS_INVALID_DATA = 0x0003;
    const quint16 EIP_STATUS_INVALID_SESSION_HANDLE = 0x0064;
    const quint16 EIP_STATUS_INVALID_LENGTH = 0x0065;
    const quint16 EIP_STATUS_UNSUPPORTED_ENCAP_PROTOCOL = 0x0069;
}

namespace CIPGeneralStatus
{
    const quint8 SUCCESS = 0x00;
    const quint8 CONNECTION_FAILURE = 0x01;
    const quint8 RESOURCE_UNAVAILABLE = 0x02;
    const quint8 INVALID_PARAMEER_VALUE = 0x03;
    const quint8 PATH_SEGMENT_ERROR = 0x04;
    const quint8 PATH_DESTINATION_UNKNOWN = 0x05;
    const quint8 PARTIAL_TRANSFER = 0x06;
    const quint8 CONNECTION_LOST = 0x07;
    const quint8 SERVICE_NOT_SUPPORTED = 0x08;
    const quint8 INVALID_ATTRIBUTE_VALUE = 0x09;
    const quint8 ATTRIBUTE_LIST_ERROR = 0x0A;
    const quint8 ALREADY_IN_REQUESTED_MODE = 0x0B;
    const quint8 OBJECT_STATE_CONFLICT = 0x0C;
    const quint8 OBJECT_ALREADY_EXISTS = 0x0D;
    const quint8 ATTRIBUTE_NOT_SETTABLE = 0x0E;
    const quint8 PRIVILEGE_VIOLATION = 0x0F;
    const quint8 DEVICE_STATE_CONFLICT = 0x10;
    const quint8 REPLY_DATA_TOO_LARGE = 0x11;
    const quint8 FRAGMENTATION_OF_PRIMITIVE_VALUE = 0x12;
    const quint8 NOT_ENOUGH_DATA = 0x13;
    const quint8 ATTRIBUTE_NOT_SUPPORTED = 0x14;
    const quint8 TOO_MUCH_DATA = 0x15;
    const quint8 OBJECT_DOES_NOT_EXIST = 0x16;
    const quint8 SERVICE_FRAGMENTATION_SEQUENCE_NOT_IN_PROGRESS = 0x17;
    const quint8 NO_STORED_ATTRIBUTE_DATA = 0x18;
    const quint8 STORE_OPERATION_FAILURE = 0x19;
    const quint8 ROUTING_FAILURE_REQUEST_PACKAGE_TOO_LARGE = 0x1A;
    const quint8 ROUTING_FAILURE_RESPONSE_PACKET_TOO_LARGE = 0x1B;
    const quint8 MISSING_ATTRIBUTE_LIST_ENTRY_DATA = 0x1C;
    const quint8 INVALID_ATTRIBUTE_VALUE_LIST = 0x1D;
    const quint8 EMBEDDED_SERVICE_ERROR = 0x1E;
    const quint8 VENDOR_SPECIFIC_ERROR = 0x1F;
    const quint8 INVALID_PARAMETER = 0x20;
    const quint8 WRITE_ONCE_VALUE_OR_MEDIUM_ALREADY_WRITTEN = 0x21;
    const quint8 INVALID_REPLY_RECEIVED = 0x22;
    const quint8 KEY_FAILURE_IN_PATH = 0x25;
    const quint8 PATH_SIZE_INVALID = 0x26;
    const quint8 UNEXPECTED_ATTRIBUTE_IN_LIST = 0x27;
    const quint8 INVALID_MEMBER_ID = 0x28;
    const quint8 MEMBER_NOT_SETTABLE = 0x29;
}

namespace CIPServiceCode
{
    const quint8 CIP_SERVICE_GET_ATTRIBUTES_ALL = 0x01;
    const quint8 CIP_SERVICE_SET_ATTRIBUTES_ALL = 0x02;
    const quint8 CIP_SERVICE_RESET = 0x05;
    const quint8 CIP_SERVICE_GET_ATTRIBUTE_SINGLE = 0x0E;
    const quint8 CIP_SERVICE_SET_ATTRIBUTE_SINGLE = 0x10;
    const quint8 CIP_SERVICE_CREATE = 0x08;
    const quint8 CIP_SERVICE_DELETE = 0x09;
    const quint8 CIP_SERVICE_RESTORE = 0x15;
    const quint8 CIP_SERVICE_KICK_TIMER = 0x4B;
    const quint8 CIP_SERVICE_OPEN_CONNECTION = 0x4C;
    const quint8 CIP_SERVICE_CLOSE_CONNECTION = 0x4D;
    const quint8 CIP_SERVICE_STOP_CONNECTION = 0x4E;
    const quint8 CIP_SERVICE_CHANGE_START = 0x4F;
    const quint8 CIP_SERVICE_GET_STATUS = 0x50;
    const quint8 CIP_SERVICE_SET_CHANGE_COMPLETE = 0x51;
    const quint8 CIP_SERVICE_AUDIT_CHANGES = 0x52;
}

namespace CIPItemID
{
    const quint16 NULL_ADDRESS = 0x0000;
    const quint16 CONNECTED_ITEM_ADDRESS = 0x00A1;
    const quint16 SEQUENCED_ITEM_ADDRESS = 0x8002;
    const quint16 CONNECTED_ITEM_DATA = 0x00B1;
    const quint16 UNCONNECTED_ITEM_DATA = 0x00B2;
    const quint16 LIST_IDENTITY_RESPONSE = 0x000C;
    const quint16 LIST_SERVICES_RESPONSE = 0x0100;
    const quint16 SOCKADDR_INFO_O_TO_T = 0x8000;
    const quint16 SOCKADDR_INFO_T_TO_O = 0x8001;

}

namespace CIPSegmentType
{
    const quint8 PORT_SEGMENT = 0; //0b00000000;
    const quint8 LOGICAL_SEGMENT = 0x20; //0b00100000;
    const quint8 NETWORK_SEGMENT = 0x40; //0b01000000;
    const quint8 SYMBOLIC_SEGMENT = 0x60; //0b01100000;
    const quint8 DATA_SEGMENT = 0x80; //0b10000000;
    const quint8 DATA_TYPE_CONSTRUCTED = 0xA0;//0x0b10100000;
    const quint8 DATA_TYPE_ELEMENTARY = 0xC0; //0b11000000;
}

namespace CIPSegmentLogicalType
{
    const quint8 CLASS_ID = 0; //0b00000000;
    const quint8 INSTANCE_ID = 0x04;//0b00000100;
    const quint8 MEMBER_ID = 0x08;//0b00001000;
    const quint8 CONNECTION_POINT = 0x0C; //0b00001100;
    const quint8 ATTRIBUTE_ID = 0x10; //0b00010000;
    const quint8 SPECIAL = 0x14; //0b00010100;
    const quint8 SERVICE_ID = 0x18; //0b00011000;
}

namespace CIPSegmentLogicalFormat
{
    const quint8 BIT_8 = 0; //0b00000000;
    const quint8 BIT_16 = 0x01; //0b00000001;
    const quint8 BIT_32 = 0x02; //0b00000010;
}

namespace CIPCommonLogicalSegment
{
    const quint8 CLASS_SEGMENT_8 =
        CIPSegmentType::LOGICAL_SEGMENT |
        CIPSegmentLogicalType::CLASS_ID |
        CIPSegmentLogicalFormat::BIT_8;

    const quint8 INSTANCE_SEGMENT_8 =
        CIPSegmentType::LOGICAL_SEGMENT |
        CIPSegmentLogicalType::INSTANCE_ID |
        CIPSegmentLogicalFormat::BIT_8;

    const quint8 ATTRIBUTE_SEGMENT_8 =
        CIPSegmentType::LOGICAL_SEGMENT |
        CIPSegmentLogicalType::ATTRIBUTE_ID |
        CIPSegmentLogicalFormat::BIT_8;
}


#endif // EIP_CIP_H

#ifndef EIP_CIP_H
#define EIP_CIP_H

#include <QHostAddress>
#include <nx/network/socket.h>
#include <cstdint>
#include <nx/network/abstract_socket.h>

typedef quint16 eip_status_t;
typedef quint8 cip_general_status_t;
typedef quint32 eip_session_handle_t;

const unsigned int kDefaultEipPort = 44818;

struct EIPEncapsulationHeader
{
    quint16 commandCode;
    quint16 dataLength;
    quint32 sessionHandle;
    quint32 status;
    quint64 senderContext;
    quint32 options;

    static const size_t SIZE =
        sizeof(decltype(commandCode)) +
        sizeof(decltype(dataLength)) +
        sizeof(decltype(sessionHandle)) +
        sizeof(decltype(status)) +
        sizeof(decltype(senderContext)) +
        sizeof(decltype(options));

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
    const quint16 kEipCommandNop = 0x0000;
    const quint16 kEipCommandListServices = 0x0004;
    const quint16 kEipCommandListIdentity = 0x0063;
    const quint16 kEipCommandListInterfaces = 0x0064;
    const quint16 kEipCommandRegisterSession = 0x0065;
    const quint16 kEipCommandUnregisterSession = 0x0066;
    const quint16 kEipCommandSendRRData = 0x006F;
    const quint16 kEipCommandSendUnitData = 0x0070;
    const quint16 kEipCommandIndicateStatus = 0x0072;
    const quint16 kEipCommandCancel = 0x0073;
}

namespace EIPStatus
{
    const quint16 kEipStatusSuccess = 0x0000;
    const quint16 kEipStatusInvalidCommand = 0x0001;
    const quint16 kEipStatusInsufficientMemory = 0x0002;
    const quint16 kEipStatusInvalidData = 0x0003;
    const quint16 kEipStatusInvalidSessionHandle = 0x0064;
    const quint16 kEipStatusInvalidLength = 0x0065;
    const quint16 kEipStatusUnsupportedEncapProtocol = 0x0069;
}

namespace CIPGeneralStatus
{
    const quint8 kSuccess = 0x00;
    const quint8 kConnectionFailure = 0x01;
    const quint8 kResourceUnavailable = 0x02;
    const quint8 kInvalidParameterValue = 0x03;
    const quint8 kPathSegmentError = 0x04;
    const quint8 kPathDestinationUnknown = 0x05;
    const quint8 kPartialTransfer = 0x06;
    const quint8 kConnectionLost = 0x07;
    const quint8 kServiceNotSupported = 0x08;
    const quint8 kInvalidAttributeValue = 0x09;
    const quint8 kAttributeListError = 0x0A;
    const quint8 kAlreadyInRequestedMode = 0x0B;
    const quint8 kObjectStateConflict = 0x0C;
    const quint8 kObjectAlreadyExists = 0x0D;
    const quint8 kAttributeNotSettable = 0x0E;
    const quint8 kPrivilegeViolation = 0x0F;
    const quint8 kDeviceStateConflict = 0x10;
    const quint8 kReplyDataTooLarge = 0x11;
    const quint8 kFragmentationOfPrimitiveValue = 0x12;
    const quint8 kNotEnoughData = 0x13;
    const quint8 kAttributeNotSupported = 0x14;
    const quint8 kTooMuchData = 0x15;
    const quint8 kObjectDoesNotExist = 0x16;
    const quint8 kServiceFragmentationSequenceNotInProgress = 0x17;
    const quint8 kNoStoredAttributeData = 0x18;
    const quint8 kStoreOperationFailure = 0x19;
    const quint8 kRoutingFailureRequestPackageTooLarge = 0x1A;
    const quint8 kRoutingFailureResponsePacketTooLarge = 0x1B;
    const quint8 kMissingAttributeListEntryData = 0x1C;
    const quint8 kInvalidAttributeValueList = 0x1D;
    const quint8 kEmbeddedServiceError = 0x1E;
    const quint8 kVendorSpecificError = 0x1F;
    const quint8 kInvalidParameter = 0x20;
    const quint8 kWriteOnceValueOrMediumAlreadyWritten = 0x21;
    const quint8 kInvalidReplyRecieved = 0x22;
    const quint8 kKeyFailureInPath = 0x25;
    const quint8 kPathSizeInvalid = 0x26;
    const quint8 kUnexpectedAttributeInList = 0x27;
    const quint8 kInvalidMemberId = 0x28;
    const quint8 kMemberNotSettable = 0x29;
}

namespace CIPServiceCode
{
    const quint8 kGetAttributesAll = 0x01;
    const quint8 kSetAttributesAll = 0x02;
    const quint8 kReset = 0x05;
    const quint8 kGetAttributeSingle = 0x0E;
    const quint8 kSetAttributeSingle = 0x10;
    const quint8 kCreate = 0x08;
    const quint8 kDelete = 0x09;
    const quint8 kRestore = 0x15;
    const quint8 kKickTimer = 0x4B;
    const quint8 kOpenConnection = 0x4C;
    const quint8 kCloseConnection = 0x4D;
    const quint8 kStopConnection = 0x4E;
    const quint8 kChangeStart = 0x4F;
    const quint8 kGetStatus = 0x50;
    const quint8 kSetChangeComplete = 0x51;
    const quint8 kAuditChange = 0x52;
}

namespace CIPItemID
{
    const quint16 kNullAddress = 0x0000;
    const quint16 kConnectedItemAddress = 0x00A1;
    const quint16 kSequencedItemAddress = 0x8002;
    const quint16 kConnectedItemData = 0x00B1;
    const quint16 kUnconnectedItemData = 0x00B2;
    const quint16 kListIdentityResponse = 0x000C;
    const quint16 kListServicesResponse = 0x0100;
    const quint16 kSockaddrInfoOToT = 0x8000;
    const quint16 kSockaddrInfoTToO = 0x8001;

}

namespace CIPSegmentType
{
    const quint8 kPortSegment = 0; //0b00000000;
    const quint8 kLogicalSegment = 0x20; //0b00100000;
    const quint8 kNetworkSegment = 0x40; //0b01000000;
    const quint8 kSymbolicSegment = 0x60; //0b01100000;
    const quint8 kDataSegment = 0x80; //0b10000000;
    const quint8 kDataTypeConstructed = 0xA0;//0x0b10100000;
    const quint8 kDataTypeElementary = 0xC0; //0b11000000;
}

namespace CIPSegmentLogicalType
{
    const quint8 kClassId = 0; //0b00000000;
    const quint8 kInstanceId = 0x04;//0b00000100;
    const quint8 kMemberId = 0x08;//0b00001000;
    const quint8 kConnectionPoint = 0x0C; //0b00001100;
    const quint8 kAttributeId = 0x10; //0b00010000;
    const quint8 kSpecial = 0x14; //0b00010100;
    const quint8 kServiceId = 0x18; //0b00011000;
}

namespace CIPSegmentLogicalFormat
{
    const quint8 kBit8 = 0; //0b00000000;
    const quint8 kBit16 = 0x01; //0b00000001;
    const quint8 kBit32 = 0x02; //0b00000010;
}

namespace CIPCommonLogicalSegment
{
    const quint8 kClassSegment8 =
        CIPSegmentType::kLogicalSegment |
        CIPSegmentLogicalType::kClassId |
        CIPSegmentLogicalFormat::kBit8;

    const quint8 kInstanceSegment8 =
        CIPSegmentType::kLogicalSegment |
        CIPSegmentLogicalType::kInstanceId |
        CIPSegmentLogicalFormat::kBit8;

    const quint8 kAttributeSegment8 =
        CIPSegmentType::kLogicalSegment |
        CIPSegmentLogicalType::kAttributeId |
        CIPSegmentLogicalFormat::kBit8;
}


#endif // EIP_CIP_H

#ifndef __RC_TYPE_H__
#define __RC_TYPE_H__

#define IN
#define OUT
#define INOUT

typedef unsigned char Byte;     ///< 8-bits unsigned
typedef unsigned short Word;    ///< 16-bits unsigned
typedef unsigned long Dword;    ///< 32-bits unsigned
typedef short Short;            ///< 16-bits signed
typedef long Long;              ///< 32-bits signed

struct ReturnChannelError
{
    enum
    {
        NO_ERROR =                  0,
        CMD_NOT_SUPPORTED =			0x05000001ul,
        //BUS_INIT_FAIL =			0x05000002ul,
        //Reply_WRONG_CHECKSUM =	0x05000036ul,
        //GET_WRONG_LENGTH =		0x05000037ul,
        Reply_WRONG_LENGTH =		0x05000038ul,
        //CMD_WRONG_LENGTH =		0x05000039ul,
        CMD_READ_FAIL =				0x05000003ul,
        //USER_INVALID =			0x05000004ul,
        //PASSWORD_INVALID =		0x05000005ul,
        //UNDEFINED_INVALID =		0x05000006ul,
        //STRING_WRONG_LENGTH =		0x05000007ul,
        //EVENT_QUEUE_ERROR	=		0x05000008ul,
        //CMD_REPEAT_ERROR =		0x05000009ul,
        //CMD_RETURN_INDEX_ERROR =	0x0500000aul,
        CMD_CONTENT_ERROR =			0x0500000bul,
    };
};

typedef enum
{
    LineDetector = 0,
    FieldDetector,
    DeclarativeMotionDetector,
    CountingRule,
    CellMotionDetector
} Event;

#endif

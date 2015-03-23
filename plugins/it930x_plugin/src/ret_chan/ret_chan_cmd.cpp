#include "ret_chan_cmd.h"
#include "ret_chan_user.h"

/**
 * The type defination of 32-bits signed type.
 */
typedef unsigned int Float;

//

static void SWordToshort(unsigned short SWordNum, short * num)
{
    short out = 0;
    out = *((short*)&SWordNum);

    (*num) = out;
}

static void shortToSWord(short num, unsigned short * SWordNum)
{
    unsigned short outSWord = 0;
    outSWord = *((unsigned short*)&num);

    (* SWordNum) = outSWord;
}

unsigned Cmd_StringReset(const Byte* buf , unsigned bufferLength, RCString* dstStr)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	error = Cmd_StringClear(dstStr);
    if (error)
        return error;
	Cmd_StringSet(buf, bufferLength, dstStr);
	return error;
}

unsigned Cmd_StringResetCopy(const RCString* srcStr , RCString* dstStr)		//clear str and read data form str and assign to str (String structure)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	error = Cmd_StringClear(dstStr);
    if (error)
        return error;
	error = Cmd_StringCopy(srcStr, dstStr);
	return error;
}

unsigned Cmd_StringCopy(const RCString* srcStr , RCString* dstStr)		//read data form str and assign to str (String structure)
{
	dstStr->stringLength = srcStr->stringLength;
	dstStr->stringData = (Byte*)User_memory_allocate( dstStr->stringLength* sizeof(Byte));

	User_memory_copy(dstStr->stringData, srcStr->stringData, dstStr->stringLength);

    return ReturnChannelError::NO_ERROR;
}

unsigned Cmd_StringSet(const Byte* buf , unsigned bufferLength, RCString* str)		//read data form buf and assign to str (String structure)
{
	str->stringLength = bufferLength;
    str->stringData = (Byte*)User_memory_allocate( bufferLength* sizeof(Byte));
    User_memory_copy( str->stringData, buf, bufferLength);

    return ReturnChannelError::NO_ERROR;
}

unsigned Cmd_StringClear(RCString* str)				//clear str (String structure)
{
    if(str->stringData!= NULL)
        User_memory_free(str->stringData) ;
    str->stringLength = 0;
    str->stringData = NULL;

    return ReturnChannelError::NO_ERROR;
}

unsigned Cmd_BytesRead(const Byte* buf , unsigned bufferLength, unsigned* index, Byte* bufDst, unsigned dstLength)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex + dstLength)
	{
        error = ReturnChannelError::CMD_READ_FAIL;

		User_memory_set( bufDst, 0xFF, dstLength);
#if Debug_check_error
        printf("BytesRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
	}else
	{
		User_memory_copy( bufDst, buf + tempIndex, dstLength);

		(* index) = (* index) +dstLength;
	}
	return error;
}

unsigned Cmd_ByteRead(const Byte* buf , unsigned bufferLength, unsigned* index, Byte* var, Byte defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +1)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("ByteRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
    }
    else
	{
		(* var) = buf[tempIndex];
		(* index) = (* index) +1;
	}
	return error;
}

unsigned Cmd_WordRead(const Byte* buf , unsigned bufferLength, unsigned* index, Word* var, Word defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +2)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("WordRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
    }
    else
	{
		(* var) = (buf[tempIndex]<<8) | buf[tempIndex+1] ;
		(* index) = (* index) + 2;
	}
	return error;
}

unsigned Cmd_DwordRead(const Byte* buf , unsigned bufferLength, unsigned* index, unsigned* var, unsigned  defaultValue)
{
	unsigned tempIndex = 0;
    unsigned error = ReturnChannelError::NO_ERROR;

	tempIndex = (* index);
	if(bufferLength< tempIndex +4)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("DwordRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
	}else
	{
		(* var) = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;
		(* index) = (* index) + 4;
	}

    return error;
}

unsigned Cmd_FloatRead(const Byte* buf , unsigned bufferLength, unsigned* index, float* var, float  defaultValue)
{
	unsigned tempIndex = 0;
	Float tempFloat = 0;
    unsigned error = ReturnChannelError::NO_ERROR;
	Byte *unsignedint_ptr = (Byte *)&tempFloat;
	Byte *float_ptr = (Byte *)var;

	tempIndex = (* index);
	if(bufferLength< tempIndex +4)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("FloatRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
    }
    else
	{
		tempFloat = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;

		float_ptr[0] = unsignedint_ptr[0];
		float_ptr[1] = unsignedint_ptr[1];
		float_ptr[2] = unsignedint_ptr[2];
		float_ptr[3] = unsignedint_ptr[3];

		(* index) = (* index) + 4;
	}
	return error;
}

unsigned Cmd_ShortRead(const Byte* buf , unsigned bufferLength, unsigned* index, short* var, short defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned tempIndex = 0;
	unsigned short tempSWord = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +2)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("ShortRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
	}else
	{

		tempSWord = (buf[tempIndex]<<8) | buf[tempIndex+1] ;
		SWordToshort( tempSWord , var);
		(* index) = (* index) + 2;
	}
	return error;
}

unsigned Cmd_CharRead(const Byte* buf , unsigned bufferLength, unsigned* index, char* var, char defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +1)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("CharRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
    }
    else
	{
		(* var) = buf[tempIndex];
		(* index) = (* index) +1;
	}
	return error;
}

void Cmd_CheckByteIndexRead(const Byte* buf , unsigned length, unsigned* var)
{
	(* var) = (buf[length]<<24) | (buf[length+1]<<16) | (buf[length+2]<<8) | buf[length+3] ;
}

unsigned Cmd_QwordRead(const Byte* buf , unsigned bufferLength, unsigned* index, unsigned long long * var, unsigned long long defaultValue)
{
	unsigned tempIndex = 0;
	Byte i = 0;
    unsigned error = ReturnChannelError::NO_ERROR;

	tempIndex = (* index);
	if(bufferLength< tempIndex +8)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
        printf("QwordRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
    }
    else
	{
		for( i = 0; i < 8; i ++)
		{
			(* var) =  (* var)  << 8;
			(* var)  =  (* var) | buf[tempIndex+i];
		}
		(* index) = (* index) + 8;
	}
	return error;
}

unsigned Cmd_StringRead(const Byte* buf , unsigned bufferLength, unsigned* index, RCString* str)
{
	unsigned tempIndex = 0;
    unsigned error = ReturnChannelError::NO_ERROR;
	unsigned strLength = 0;
	tempIndex = (* index);

    if (bufferLength < tempIndex + 4)
	{
        error = ReturnChannelError::CMD_READ_FAIL;
		str->stringLength = 0;
		str->stringData = NULL;
#if Debug_check_error
        printf("StringRead fail error = %lx\n", ReturnChannelError::CMD_READ_FAIL);
#endif
	}else
	{
		strLength = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;
		tempIndex = tempIndex + 4;

		if(bufferLength < tempIndex + strLength)
		{
            error = ReturnChannelError::CMD_READ_FAIL;
			str->stringLength = 0;
			str->stringData = NULL;
		}else
		{
			str->stringLength = strLength;
			str->stringData = (Byte*) User_memory_allocate(strLength * sizeof(Byte));

			User_memory_copy( str->stringData, buf + tempIndex, strLength);

			tempIndex = tempIndex + str->stringLength;
			(* index) = tempIndex;
		}
	}
	return error;
}

void Cmd_WordAssign(Byte* buf,Word var,unsigned* length)
{
	unsigned tempLength =0;
	tempLength = (* length);
	buf[tempLength+0] = (Byte)(var>>8);
	buf[tempLength+1] = (Byte)(var);

	(*length) = (*length) + 2;
}

void Cmd_DwordAssign(Byte* buf,unsigned var,unsigned* length)
{
	unsigned tempLength =0;
	tempLength = (* length);
	buf[tempLength+0] = (Byte)(var>>24);
	buf[tempLength+1] = (Byte)(var>>16);
	buf[tempLength+2] = (Byte)(var>>8);
	buf[tempLength+3] = (Byte)(var);

	(*length) = (*length) + 4;
}

void Cmd_QwordAssign(Byte* buf, unsigned long long var,unsigned* length)
{
	unsigned tempLength =0;
	tempLength = (* length);
	buf[tempLength+0] = (Byte)(var>>56);
	buf[tempLength+1] = (Byte)(var>>48);
	buf[tempLength+2] = (Byte)(var>>40);
	buf[tempLength+3] = (Byte)(var>>32);
	buf[tempLength+4] = (Byte)(var>>24);
	buf[tempLength+5] = (Byte)(var>>16);
	buf[tempLength+6] = (Byte)(var>>8);
	buf[tempLength+7] = (Byte)(var);
	(*length) = (*length) + 8;
}

void Cmd_StringAssign(Byte* buf,const RCString* str,unsigned* length)
{
	unsigned tempLength = (* length);
	unsigned strLength = str->stringLength;
	buf[tempLength + 0] = (Byte)(strLength>>24);
	buf[tempLength + 1]  = (Byte)(strLength>>16);
	buf[tempLength + 2] = (Byte)(strLength>>8);
	buf[tempLength + 3]  = (Byte)strLength;

	tempLength = tempLength + 4;

	User_memory_copy( buf + tempLength, str->stringData, strLength);

	tempLength = tempLength + strLength;
	(*length) = tempLength;
}

void Cmd_FloatAssign(Byte* buf,float var,unsigned* length)
{
	Float tempFloat = 0;
	Byte *unsignedint_ptr = (Byte *)&tempFloat;
	Byte *float_ptr = (Byte *)&var;
	unsigned tempLength =0;

	tempLength = (* length);

	unsignedint_ptr[0] = float_ptr[0];
	unsignedint_ptr[1] = float_ptr[1];
	unsignedint_ptr[2] = float_ptr[2];
	unsignedint_ptr[3] = float_ptr[3];

	//Big Endian
	buf[tempLength+0] = (Byte)(tempFloat>>24);
	buf[tempLength+1] = (Byte)(tempFloat>>16);
	buf[tempLength+2] = (Byte)(tempFloat>>8);
	buf[tempLength+3] = (Byte)(tempFloat);

	(*length) = (*length) + 4;
}

void Cmd_ShortAssign(Byte* buf,short var,unsigned* length)
{
	unsigned tempLength =0;
	unsigned short tempSWord;

	shortToSWord( var, &tempSWord);

	tempLength = (* length);
	buf[tempLength+0] = (Byte)(tempSWord>>8);
	buf[tempLength+1] = (Byte)(tempSWord);

	(*length) = (*length) + 2;
}

#include "ret_chan_cmd.h"
#include "ret_chan_user.h"


unsigned full(Byte front, Byte rear, Byte size)
{
	return (((front+1)%size)==rear);
}

unsigned empty(Byte front, Byte rear)
{
	return (rear==front);
}

unsigned enqueue(EventQueue* eventQueue, Byte item )
{
	unsigned	error = ModulatorError_NO_ERROR;
	Byte tmpFront = 0;
	Byte tmpRear = 0;
	Byte size = 0;
	size = sizeof(eventQueue->queue);
	tmpFront  = eventQueue->front;
	tmpRear  = eventQueue->rear;

	if(full(tmpFront, tmpRear, size) == 0)
	{
		eventQueue->queue[tmpFront] = item;
		tmpFront =(tmpFront+1)%size;
		eventQueue->front = tmpFront;
	}
	else
	{
		printf("Queue is full!\n");
		error = ReturnChannelError_EVENT_QUEUE_ERROR;
	}
	return error;
}

unsigned dequeue(EventQueue* eventQueue, Byte* item )
{
	unsigned	error = ModulatorError_NO_ERROR;
	Byte tmpFront = 0;
	Byte tmpRear = 0;
	Byte tmpitem = 0;
	Byte size = 0;
	size = sizeof(eventQueue->queue);
	tmpFront  = eventQueue->front;
	tmpRear  =  eventQueue->rear;


	if(empty(tmpFront, tmpRear) == 0)
	{
		tmpitem = eventQueue->queue[tmpRear];
		tmpRear= (tmpRear+1)%size;
		(*item) = tmpitem;
		eventQueue->rear = tmpRear;
	}else
	{
		printf("Queue is empty!\n");
		error = ReturnChannelError_EVENT_QUEUE_ERROR;
	}
	return error;
}

void initQueue(EventQueue* queue)
{
	queue->front = 0;
	queue->rear = 0;
}

void SWordToshort(unsigned short SWordNum,short* num)
{
	short out = 0;
	out = *((short*)&SWordNum);

	(*num) = out;
}

void shortToSWord(short num, unsigned short * SWordNum)
{
	unsigned short outSWord = 0;
	outSWord = *((unsigned short*)&num);

	(* SWordNum) = outSWord;
}

unsigned Cmd_splitBuffer(Byte* buf, unsigned length, Byte* splitBuf, unsigned splitLength,unsigned* start )
{
	unsigned offset = (*start);
	User_memory_set(splitBuf,0, splitLength);
	if(length-offset>splitLength)
	{
		User_memory_copy(splitBuf, buf + offset, splitLength);

		offset = offset + splitLength;
		(*start) = offset;

		return (length-offset);
	}else
	{
		User_memory_copy(splitBuf, buf + offset, length - offset);

		(*start)  = length;
		return 0;
	}
}

unsigned Cmd_addTS(Byte* dstbuf, Word PID, Byte seqNum)
{
	unsigned	error = ModulatorError_NO_ERROR;

	//TS header
	dstbuf[0] = 0x47;													//svnc_byte
	dstbuf[1] = 0x20 | ((Byte) (PID >> 8));					//TEI + .. +PID
	dstbuf[2] = (Byte) PID;											//PID
	dstbuf[3] = 0x10| ((Byte) (seqNum & 0x0F));		//..... + continuity counter

	return error;
}

unsigned Cmd_sendTSCmd(Byte* buffer, unsigned bufferSize, Device* device, int *txLen, CmdSendConfig* cmdSendConfig)
{
	unsigned error = ModulatorError_NO_ERROR;
	Byte TSPkt[188]={0xFF};
	Byte RCCmdData[RCMaxSize]={0xFF};
	unsigned start = 0;
	unsigned total_pktCount = 0,pktCount = 1;
	unsigned dwRemainder = 0;

	if(bufferSize <= 0)
	{
		error = ReturnChannelError_CMD_WRONG_LENGTH;
		return error;
	}

	Word tmpcommand = buffer[4]<<8 | buffer[5];

	if( device->clientTxDeviceID == 0xFFFF)
	{
		switch(tmpcommand)
		{
		case CMD_GetTxDeviceAddressIDInput:
			break;
		default:
			error = ReturnChannelError_CMD_TXDEVICEID_ERROR;
			return error;
		}
	}

	//unsigned sleepTime = cmdSendConfig->byRepeatPacketTimeInterval * 1000;
	User_mutex_lock();

	dwRemainder = bufferSize%RCMaxSize;

	if(dwRemainder == 0)
		total_pktCount = (bufferSize/RCMaxSize);
	else
		total_pktCount = (bufferSize/RCMaxSize)+1;

	while(Cmd_splitBuffer(buffer,bufferSize,RCCmdData, RCMaxSize,&start)!=0)
	{
		device->RCCmd_sequence++;
		device->TS_sequence++;

		if(device->bIsDebug)
			printf("RCCmd_sequence = %u\n",device->RCCmd_sequence);

		error = Cmd_addTS(TSPkt, device->PID, device->TS_sequence);
		if(error) goto exit;
		if(!cmdSendConfig->bIsCmdBroadcast)
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 4);
		else
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 4);
		if(error) goto exit;
		error = User_returnChannelBusTx(sizeof(TSPkt), TSPkt, txLen);
		if(error) goto exit;
		pktCount++;
	}
	device->RCCmd_sequence++;
	device->TS_sequence++;
	error = Cmd_addTS(TSPkt, device->PID, device->TS_sequence);

	if(device->bIsDebug)
		printf("RCCmd_sequence = %u\n",device->RCCmd_sequence);
	if(!cmdSendConfig->bIsCmdBroadcast)
	{
		if(dwRemainder == 0)
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 4);
		else
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, dwRemainder, 4);
	}
	else
	{
		if(dwRemainder == 0)
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 4);
		else
			error = Cmd_addRC(RCCmdData, TSPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, dwRemainder, 4);
	}
	if(error) goto exit;
	error = User_returnChannelBusTx(sizeof(TSPkt), TSPkt, txLen);
	if(error) goto exit;

exit:
	User_mutex_unlock();
	return error;
}

unsigned Cmd_addRC(Byte* srcbuf,  Byte* dstbuf, Word RXDeviceID, Word TXDeviceID, Word seqNum, unsigned total_pktNum, unsigned pktNum, unsigned pktLength, Byte index)
{
	unsigned	error = ModulatorError_NO_ERROR;

	unsigned payload_start_point = 0, payload_end_point = 0;
	//RC header
	dstbuf[index + 183] = 0x0d;
	dstbuf[index++] = '#';													//Leading Tag
	dstbuf[index++] = (Byte)(RXDeviceID>>8);
	dstbuf[index++] = (Byte)RXDeviceID;
	payload_start_point = index;
	dstbuf[index++] = (Byte)(TXDeviceID>>8);
	dstbuf[index++] = (Byte)TXDeviceID;
	dstbuf[index++] = (Byte)(total_pktNum>>8);				//total_pktNum
	dstbuf[index++] = (Byte)total_pktNum;						//total_pktNum
	dstbuf[index++] = (Byte)(pktNum>>8);						//pktNum
	dstbuf[index++] = (Byte)pktNum;									//pktNum
	dstbuf[index++] = (Byte)(seqNum>>8);						//seqNum
	dstbuf[index++] = (Byte)seqNum;									//seqNum
	dstbuf[index++] = (Byte)(pktLength>>8);						//seqNum
	dstbuf[index++] = (Byte)pktLength;								//pktLength

	User_memory_copy(dstbuf + index, srcbuf, pktLength);
	index = index + (Byte)pktLength;
	payload_end_point = index - 1;

	dstbuf[index++] = Cmd_calChecksum(payload_start_point,payload_end_point,dstbuf);

	return error;
}

unsigned Cmd_sendRCCmd(Byte* buffer, unsigned bufferSize, Device* device, int *txLen, CmdSendConfig* cmdSendConfig)
{
	unsigned	error = ModulatorError_NO_ERROR;
	Byte RCPkt[184]={0xFF};
	Byte RCCmdData[RCMaxSize]={0xFF};
	unsigned start = 0;
	unsigned total_pktCount = 0,pktCount = 1;
	Byte i = 0;
	unsigned sleepTime = 0;
	unsigned dwRemainder = 0;

	if(bufferSize <= 0)
	{
		error = ReturnChannelError_CMD_WRONG_LENGTH;
		return error;
	}

	//Word tmpcommand = buffer[4]<<8 | buffer[5];

	if( device->clientTxDeviceID == 0xFFFF)
	{
			error = ReturnChannelError_CMD_TXDEVICEID_ERROR;
			return error;
	}

	sleepTime = cmdSendConfig->byRepeatPacketTimeInterval * 1000;
	User_mutex_lock();

	dwRemainder = bufferSize%RCMaxSize;

	if(dwRemainder == 0)
		total_pktCount = (bufferSize/RCMaxSize);
	else
		total_pktCount = (bufferSize/RCMaxSize)+1;

	while(Cmd_splitBuffer(buffer,bufferSize,RCCmdData, RCMaxSize,&start)!=0)
	{
		device->RCCmd_sequence++;

		if(device->bIsDebug)
			printf("RCCmd_sequence = %u\n",device->RCCmd_sequence);
		if(!cmdSendConfig->bIsCmdBroadcast)
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 0);
		else
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 0);
		if(error) goto exit;

		if(cmdSendConfig->bIsRepeatPacket)
		{
			for( i = 0; i < cmdSendConfig->byRepeatPacketNumber; i ++)
			{
				error = User_returnChannelBusTx(sizeof(RCPkt), RCPkt, txLen);
				if (error)
                    goto exit;

				usleep(sleepTime);
			}
		}else
		{
			error = User_returnChannelBusTx(sizeof(RCPkt), RCPkt, txLen);
			if(error) goto exit;
		}
		pktCount++;
	}

	device->RCCmd_sequence++;
	if(device->bIsDebug)
		printf("RCCmd_sequence = %u\n",device->RCCmd_sequence);
	if(!cmdSendConfig->bIsCmdBroadcast)
	{
		if(dwRemainder == 0)
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 0);
		else
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, device->clientTxDeviceID, device->RCCmd_sequence, total_pktCount, pktCount, dwRemainder, 0);
	}
	else
	{
		if(dwRemainder == 0)
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, RCMaxSize, 0);
		else
			error = Cmd_addRC(RCCmdData, RCPkt, device->hostRxDeviceID, 0xFFFF, device->RCCmd_sequence, total_pktCount, pktCount, dwRemainder, 0);
	}

	if (error)
        goto exit;

	if (cmdSendConfig->bIsRepeatPacket)
	{
		for( i = 0; i < cmdSendConfig->byRepeatPacketNumber; i ++)
		{
			error = User_returnChannelBusTx(sizeof(RCPkt), RCPkt, txLen);
			if (error)
                goto exit;

			usleep(sleepTime);
		}
	}
	else
	{
		error = User_returnChannelBusTx(sizeof(RCPkt), RCPkt, txLen);
		if(error) goto exit;
	}

exit:
	User_mutex_unlock();
	return error;
}

unsigned Cmd_StringReset(const Byte* buf , unsigned bufferLength, RCString* dstStr)
{
	unsigned	error = ModulatorError_NO_ERROR;
	error = Cmd_StringClear(dstStr);
	if(error) return error;
	Cmd_StringSet(buf, bufferLength, dstStr);
	return error;
}

unsigned Cmd_StringResetCopy(const RCString* srcStr , RCString* dstStr)		//clear str and read data form str and assign to str (String structure)
{
	unsigned	error = ModulatorError_NO_ERROR;
	error = Cmd_StringClear(dstStr);
	if(error) return error;
	error = Cmd_StringCopy(srcStr, dstStr);
	return error;
}

unsigned Cmd_StringCopy(const RCString* srcStr , RCString* dstStr)		//read data form str and assign to str (String structure)
{
	unsigned	error = ModulatorError_NO_ERROR;

	dstStr->stringLength = srcStr->stringLength;
	dstStr->stringData = (Byte*)User_memory_allocate( dstStr->stringLength* sizeof(Byte));

	User_memory_copy(dstStr->stringData, srcStr->stringData, dstStr->stringLength);

	return error;
}

unsigned Cmd_StringSet(const Byte* buf , unsigned bufferLength, RCString* str)		//read data form buf and assign to str (String structure)
{
	unsigned	error = ModulatorError_NO_ERROR;

	str->stringLength = bufferLength;
    str->stringData = (Byte*)User_memory_allocate( bufferLength* sizeof(Byte));
    User_memory_copy( str->stringData, buf, bufferLength);

	return error;
}

unsigned Cmd_StringClear(RCString* str)				//clear str (String structure)
{
	unsigned error = ModulatorError_NO_ERROR;

    if(str->stringData!= NULL)
        User_memory_free(str->stringData) ;
    str->stringLength = 0;
    str->stringData = NULL;
	return error;
}

unsigned Cmd_BytesRead(const Byte* buf , unsigned bufferLength, unsigned* index, Byte* bufDst, unsigned dstLength)
{
	unsigned error = ModulatorError_NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex + dstLength)
	{
		error = ReturnChannelError_CMD_READ_FAIL;

		User_memory_set( bufDst, 0xFF, dstLength);
#if Debug_check_error
		printf("BytesRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
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
	unsigned error = ModulatorError_NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +1)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("ByteRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
	{
		(* var) = buf[tempIndex];
		(* index) = (* index) +1;
	}
	return error;
}

unsigned Cmd_WordRead(const Byte* buf , unsigned bufferLength, unsigned* index, Word* var, Word defaultValue)
{
	unsigned error = ModulatorError_NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +2)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("WordRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
	{
		(* var) = (buf[tempIndex]<<8) | buf[tempIndex+1] ;
		(* index) = (* index) + 2;
	}
	return error;
}

unsigned Cmd_DwordRead(const Byte* buf , unsigned bufferLength, unsigned* index, unsigned* var, unsigned  defaultValue)
{
	unsigned tempIndex = 0;
	unsigned error = ModulatorError_NO_ERROR;

	tempIndex = (* index);
	if(bufferLength< tempIndex +4)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("DwordRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
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
	unsigned error = ModulatorError_NO_ERROR;
	Byte *unsignedint_ptr = (Byte *)&tempFloat;
	Byte *float_ptr = (Byte *)var;

	tempIndex = (* index);
	if(bufferLength< tempIndex +4)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("FloatRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
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
	unsigned error = ModulatorError_NO_ERROR;
	unsigned tempIndex = 0;
	unsigned short tempSWord = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +2)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("ShortRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
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
	unsigned error = ModulatorError_NO_ERROR;
	unsigned tempIndex = 0;

	tempIndex = (* index);
	if(bufferLength< tempIndex +1)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("CharRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
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
	unsigned error = ModulatorError_NO_ERROR;

	tempIndex = (* index);
	if(bufferLength< tempIndex +8)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		(* var) = defaultValue;
#if Debug_check_error
		printf("QwordRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
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
	unsigned error = ModulatorError_NO_ERROR;
	unsigned strLength = 0;
	tempIndex = (* index);

	if(bufferLength< tempIndex +4)
	{
		error = ReturnChannelError_CMD_READ_FAIL;
		str->stringLength = 0;
		str->stringData = NULL;
#if Debug_check_error
		printf("StringRead fail error = %lx\n", ReturnChannelError_CMD_READ_FAIL);
#endif
	}else
	{
		strLength = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;
		tempIndex = tempIndex + 4;

		if(bufferLength < tempIndex + strLength)
		{
			error = ReturnChannelError_CMD_READ_FAIL;
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

Byte Cmd_calChecksum(IN unsigned payload_start_point, IN unsigned payload_end_point, IN const Byte * buffer)
{
	unsigned i;
	Word Sum = 0;
	Byte Checksum = 0;

	for(i = payload_start_point;i <= payload_end_point;i++){
		Sum = Sum + buffer[i];
	}
	Checksum = (Byte)(Sum%256);
	return Checksum;
}

//-------------------TS process-------------------------

unsigned TSHeadCheck(Byte* srcbuf, Device* device)
{
	Word PID = 0;
	Byte tsSeq = 0;
	unsigned error = ModulatorError_NO_ERROR;

	if(srcbuf[0]!= 0x47)
	{
		printf("<TS syntex Error!>\n");
		error = ReturnChannelError_CMD_TS_SYNTEX_ERROR;
		return error;			//syntex error
	}

	PID = ((srcbuf[1]&0x1F)<<8) | srcbuf[2];
	tsSeq = srcbuf[3]&0x0F;

	if(device->PID!=PID)
	{
		error = ReturnChannelError_CMD_TS_SYNTEX_ERROR;
		return error;			//Not deivce
	}

	if(	device->TS_sequence != (tsSeq-1))
	{
		device->TS_sequence = tsSeq;
		printf("<TS Lost Pkt!>\n");
	}
	else
		device->TS_sequence = tsSeq;

	return 0;
}
//-------------------TS process-------------------------

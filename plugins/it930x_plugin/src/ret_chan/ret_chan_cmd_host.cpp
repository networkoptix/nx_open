#include <vector>

#include "ret_chan_user.h"
#include "ret_chan_user_host.h"
#include "ret_chan_cmd_host.h"

unsigned Cmd_HostParser(IN RCHostInfo * deviceInfo, Word command, const Byte * Buffer, unsigned bufferLength)
{
	unsigned error = ModulatorError_NO_ERROR;
    unsigned check;
	Byte i = 0, j = 0;
	unsigned index = 0;
	unsigned checkByte = 0;
	Rule_LineDetector * ptrRule_LineDetector = NULL;
	Rule_FieldDetector * ptrRule_FieldDetector = NULL;
	Rule_MotionDetector * ptrRule_MotionDetector = NULL;
	Rule_Counting * ptrRule_Counting = NULL;
	Rule_CellMotion * ptrRule_CellMotion = NULL;
	Byte tmpByte = 0;

	if (bufferLength < 8)
		error = ReturnChannelError_Reply_WRONG_LENGTH;

    if (error == ModulatorError_NO_ERROR)
        Cmd_CheckByteIndexRead(Buffer, index, &checkByte);

    if (error == ModulatorError_NO_ERROR)
    {
        index = 7;

        {
            Security security;
            check = Cmd_StringRead(Buffer, checkByte, &index, &security.userName);
            check = Cmd_StringRead(Buffer, checkByte, &index, &security.password);

            Byte valid = Valid;
            User_getSecurity(security.userName, security.password, &valid);
            switch (valid)
            {
                case UserNameInvalid:
                    return ReturnChannelError_USER_INVALID;
                case PasswordInvalid:
                    return ReturnChannelError_PASSWORD_INVALID;
                default:
                    break;
            }

            check = Cmd_StringClear(&security.userName);
            check = Cmd_StringClear(&security.password);
        }

#if 0
		if ((command == CMD_MetadataStreamOutput)|| ((deviceInfo->cmdConfig.IsTimeOut == 0) && ( deviceInfo->cmdConfig.ExpectCmd == command)))
#endif
		{
			switch (command)
			{
				//-----------HOST Receive Ack------------------
			case CMD_SetTxDeviceAddressIDOutput :
			case CMD_SetCalibrationTableOutput :
			case CMD_SetTransmissionParametersOutput :
			case CMD_SetHwRegisterValuesOutput :
			case CMD_SetAdvaneOptionsOutput :
			case CMD_SetTPSInformationOutput :
			case CMD_SetSiPsiTableOutput :
			case CMD_SetNitLocationOutput :
			case CMD_SetSdtServiceOutput :
			case CMD_SetEITInformationOutput :
			case CMD_SetSystemFactoryDefaultOutput :
			case CMD_SetHostnameOutput :
			case CMD_SetSystemDateAndTimeOutput :
			case CMD_SetOSDInformationOutput :
			case CMD_SetRelayOutputStateOutput :
			case CMD_SetRelayOutputSettingsOutput :
			case CMD_SetImagingSettingsOutput :
			case CMD_IMG_MoveOutput :
			case CMD_IMG_StopOutput :
			case CMD_SetUserDefinedSettingsOutput :
			case CMD_SetSynchronizationPointOutput :
			case CMD_SetVideoSourceConfigurationOutput:
			case CMD_SetVideoEncoderConfigurationOutput:
			case CMD_SetAudioSourceConfigurationOutput:
			case CMD_SetAudioEncoderConfigurationOutput:
			case CMD_SetVideoOSDConfigurationOutput:
			case CMD_SetVideoPrivateAreaOutput:
			case CMD_SetVideoSourceControlOutput:
			case CMD_GotoPresetOutput:
			case CMD_RemovePresetOutput:
			case CMD_AbsoluteMoveOutput:
			case CMD_RelativeMoveOutput:
			case CMD_ContinuousMoveOutput:
			case CMD_SetHomePositionOutput:
			case CMD_GotoHomePositionOutput:
			case CMD_PTZ_StopOutput:
			case CMD_CreateRuleOutput :
			case CMD_ModifyRuleOutput :
			case CMD_DeleteRuleOutput :
			case CMD_SetMetadataSettingsOutput :
				if(bufferLength >=8)
				{
					User_getGeneralReply( deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetTxDeviceAddressIDOutput :	//GetTxDeviceAddressID
				if (bufferLength >= 8)
				{
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->newTxDevice.deviceAddressID, 0xFF);
					//User_getDeviceAddressReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetTransmissionParameterCapabilitiesOutput	:	//GetTransmissionParameterCapabilitiesReply
				if(bufferLength >=23)
				{
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.bandwidthOptions, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.frequencyMin, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.frequencyMax, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.constellationOptions, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.FFTOptions, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.codeRateOptions, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.guardInterval, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.attenuationMin, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.attenuationMax, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.extensionFlag, 0);
					if(deviceInfo->transmissionParameterCapabilities.extensionFlag == 1)
					{
						check = Cmd_CharRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.attenuationMin_signed, 127);
						check = Cmd_CharRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.attenuationMax_signed, 127);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.TPSCellIDMin, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.TPSCellIDMax, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.channelNumMin, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.channelNumMax, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.bandwidthStrapping, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.TVStandard, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.segmentationMode, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.oneSeg_Constellation, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameterCapabilities.oneSeg_CodeRate, 0xFF);
					}
					User_getTransmissionParameterCapabilitiesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetTransmissionParametersOutput://get Transmission Parameter Reply
				if(bufferLength >=18)
				{
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.bandwidth, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.frequency, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.constellation, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.FFT, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.codeRate, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.interval, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.attenuation, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.extensionFlag, 0);
					if(deviceInfo->transmissionParameter.extensionFlag == 1)
					{
						check = Cmd_CharRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.attenuation_signed, 127);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.TPSCellID, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.channelNum, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.bandwidthStrapping, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.TVStandard, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.segmentationMode, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.oneSeg_Constellation, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->transmissionParameter.oneSeg_CodeRate, 0xFF);
					}
					User_getTransmissionParameterReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetHwRegisterValuesOutput://GetHwRegisterValues Reply
				if(bufferLength >=18)
				{
					User_memory_free(deviceInfo->hwRegisterInfo.registerValues);
					deviceInfo->hwRegisterInfo.registerValues = NULL;
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->hwRegisterInfo.valueListSize, 0);
					User_memory_free(deviceInfo->hwRegisterInfo.registerValues);
					deviceInfo->hwRegisterInfo.registerValues = (Byte*) User_memory_allocate(deviceInfo->hwRegisterInfo.valueListSize * sizeof(Byte));
					check = Cmd_BytesRead(Buffer , checkByte,&index, deviceInfo->hwRegisterInfo.registerValues, deviceInfo->hwRegisterInfo.valueListSize);

					User_getHwRegisterValuesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAdvanceOptionsOutput://getAdvanceOptions  Reply
				if(bufferLength >=16)
				{
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.PTS_PCR_delayTime, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.timeInterval, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.skipNumber, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.overFlowNumber, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.overFlowSize, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.extensionFlag, 0);
					if(deviceInfo->advanceOptions.extensionFlag == 1)
					{
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.Rx_LatencyRecoverTimeInterval, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.SIPSITableDuration, 0xFF);
						check = Cmd_CharRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.frameRateAdjustment, 127);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.repeatPacketMode, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.repeatPacketNum, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.repeatPacketTimeInterval, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->advanceOptions.TS_TableDisable, 0xFF);
					}
					User_getAdvanceOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetTPSInformationOutput:
				if(bufferLength >=15)
				{
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.cellID, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.highCodeRate, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.lowCodeRate, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.transmissionMode, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.constellation, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->tpsInfo.interval, 0xFF);

					User_getTPSInfoReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetSiPsiTableOutput://GetSiPsiTable Reply
				if(bufferLength >=47)
				{
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->psiTable.ONID, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->psiTable.NID, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->psiTable.TSID, 0xFF);
					check = Cmd_BytesRead(Buffer , checkByte,&index, deviceInfo->psiTable.networkName, 32);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->psiTable.extensionFlag, 0);
					if(deviceInfo->psiTable.extensionFlag == 1)
					{
						check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->psiTable.privateDataSpecifier, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->psiTable.NITVersion, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->psiTable.countryID, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->psiTable.languageID, 0xFF);
					}
					User_getSiPsiTableReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetNitLocationOutput://GetNitLocation Reply
				if(bufferLength >=16)
				{
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->nitLoacation.latitude, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->nitLoacation.longitude, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->nitLoacation.extentLatitude, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->nitLoacation.extentLongitude, 0xFF);

					User_getNitLocationReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetSdtServiceOutput://GetSdtService Reply
				if(bufferLength >=13)
				{
					check = Cmd_StringClear(&deviceInfo->serviceConfig.serviceName);
					check = Cmd_StringClear(&deviceInfo->serviceConfig.provider);

					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.serviceID, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.enable, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.LCN, 0xFF);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->serviceConfig.serviceName);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->serviceConfig.provider);
					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.extensionFlag, 0);
					if(deviceInfo->serviceConfig.extensionFlag == 1)
					{
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.IDAssignationMode, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_RegionID, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_BroadcasterRegionID, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_RemoteControlKeyID, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_ServiceIDDataType_1, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_ServiceIDDataType_2, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte, &index, &deviceInfo->serviceConfig.ISDBT_ServiceIDPartialReception, 0xFF);
					}
					User_getSdtServiceReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetEITInformationOutput://GetEITInformation Reply
				if(bufferLength >=8)
				{
					for(i = 0; i < deviceInfo->eitInfo.listSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->eitInfo.eitInfoParam[i].eventName);
						check = Cmd_StringClear(&deviceInfo->eitInfo.eitInfoParam[i].eventText);
					}
					User_memory_free(deviceInfo->eitInfo.eitInfoParam);
					deviceInfo->eitInfo.eitInfoParam = NULL;

					check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->eitInfo.listSize, 0);
					deviceInfo->eitInfo.eitInfoParam = (EITInfoParam*) User_memory_allocate(deviceInfo->eitInfo.listSize * sizeof(EITInfoParam));
					for(i = 0; i < deviceInfo->eitInfo.listSize; i ++)
					{
						check = Cmd_ByteRead(Buffer, checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].enable, 0);
						check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].startDate, 0);
						check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].startTime, 0);
						check = Cmd_DwordRead(Buffer, checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].duration, 0);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].eventName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->eitInfo.eitInfoParam[i].eventText);
					}

					User_getEITInfoReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//------------------	ccHDtv Service -----------------------------

				//------------------ Device Management Service---------------------
			case CMD_GetCapabilitiesOutput:		//GetCapabilitiesReply
				if(bufferLength >=8)
				{
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->deviceCapability.supportedFeatures, 0xFF);
					User_getDeviceCapabilityReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetDeviceInformationOutput:			//GetDeviceInformationReply
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->manufactureInfo.manufactureName);
					check = Cmd_StringClear(&deviceInfo->manufactureInfo.modelName);
					check = Cmd_StringClear(&deviceInfo->manufactureInfo.firmwareVersion);
					check = Cmd_StringClear(&deviceInfo->manufactureInfo.serialNumber);
					check = Cmd_StringClear(&deviceInfo->manufactureInfo.hardwareId);

					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->manufactureInfo.manufactureName );
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->manufactureInfo.modelName );
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->manufactureInfo.firmwareVersion );
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->manufactureInfo.serialNumber );
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->manufactureInfo.hardwareId );

					User_getDeviceInformationReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetHostnameOutput:			//GetHostnameReply
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->hostInfo.hostName);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->hostInfo.hostName );

					User_getHostnameReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetSystemDateAndTimeOutput:	//GetSystemDateAndTimeReply
				if (bufferLength >=21)
				{
					check = Cmd_BytesRead(Buffer , checkByte, &index, deviceInfo->systemTime.countryCode, 3);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.countryRegionID, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.daylightSavings, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.timeZone, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCHour, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCMinute, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCSecond, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->systemTime.UTCYear, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCMonth, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCDay, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.extensionFlag, 0);
					if(deviceInfo->systemTime.extensionFlag == 1)
					{
						check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.UTCMillisecond, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.timeAdjustmentMode, 0xFF);
						check = Cmd_DwordRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.timeAdjustmentCriterionMax, 0xFF);
						check = Cmd_DwordRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.timeAdjustmentCriterionMin, 0xFF);
						check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->systemTime.timeSyncDuration, 0xFF);
					}

					User_getSystemDateAndTimeReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetSystemLogOutput:	//GetSystemLogReply
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->systemLog.logData);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->systemLog.logData);

					User_getSystemLogReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetOSDInformationOutput:	//GetOSDInformation Reply
				if(bufferLength >=22)
				{
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.dateEnable, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.datePosition, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.dateFormat, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.timeEnable, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.timePosition, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.timeFormat, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.logoEnable, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.logoPosition, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.logooption, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.detailInfoEnable, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.detailInfoPosition, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.detailInfooption, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.textEnable, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->osdInfo.textPosition, 0xFF);

					check = Cmd_StringClear(&deviceInfo->osdInfo.text);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->osdInfo.text);

					User_getOSDInfoReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_SystemRebootOutput:	//SystemRebootReply
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->systemReboot.responseMessage);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->systemReboot.responseMessage);

					User_systemRebootReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_UpgradeSystemFirmwareOutput:
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->systemFirmware.message);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->systemFirmware.message);

					User_upgradeSystemFirmwareReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

				//-------------------Device_IO Service----------------------
			case CMD_GetDigitalInputsOutput:	//GetDigitalInputsReply
				if (bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->digitalInputsInfo.listSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->digitalInputsInfo.tokenList[i]);
					}
					User_memory_free(deviceInfo->digitalInputsInfo.tokenList);
					deviceInfo->digitalInputsInfo.tokenList = NULL;
					deviceInfo->digitalInputsInfo.listSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->digitalInputsInfo.listSize, 0);
					deviceInfo->digitalInputsInfo.tokenList = (RCString*) User_memory_allocate( deviceInfo->digitalInputsInfo.listSize * sizeof(RCString));
					for( i = 0; i < deviceInfo->digitalInputsInfo.listSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->digitalInputsInfo.tokenList[i]);
					}

					User_getDigitalInputsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetRelayOutputsOutput:	//GetRelayOutputsReply
				if(bufferLength >=9)
				{
					for(i=0; i< deviceInfo->relayOutputs.listSize; i++)
					{
						check = Cmd_StringClear(&deviceInfo->relayOutputs.relayOutputsParam[i].token);
					}
					User_memory_free(deviceInfo->relayOutputs.relayOutputsParam);
					deviceInfo->relayOutputs.relayOutputsParam = NULL;
					deviceInfo->relayOutputs.listSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->relayOutputs.listSize, 0);
					deviceInfo->relayOutputs.relayOutputsParam = (RelayOutputsParam*)User_memory_allocate(deviceInfo->relayOutputs.listSize*sizeof(RelayOutputsParam));
					for(i=0; i< deviceInfo->relayOutputs.listSize; i++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->relayOutputs.relayOutputsParam[i].token);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->relayOutputs.relayOutputsParam[i].mode, 0xFF);
						check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->relayOutputs.relayOutputsParam[i].delayTime, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->relayOutputs.relayOutputsParam[i].idleState, 0xFF);
					}

					User_getRelayOutputsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//-------------------Device_IO Service----------------------

				//-------------------Imaging Service-------------------------
			case CMD_GetImagingSettingsOutput:	//GetImagingSettingsReply
				if(bufferLength >=108)
				{
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.backlightCompensationMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.backlightCompensationLevel, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.brightness, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.colorSaturation, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.contrast, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.exposureMode, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.exposurePriority, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureWindowbottom, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureWindowtop, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureWindowright, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureWindowleft, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.minExposureTime, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.maxExposureTime, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureMinGain, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureMaxGain, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureMinIris, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureMaxIris, 0.0);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureTime, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureGain, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.exposureIris, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.autoFocusMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.focusDefaultSpeed, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.focusNearLimit, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.focusFarLimit, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.irCutFilterMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.sharpness, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.wideDynamicRangeMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.wideDynamicRangeLevel, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.whiteBalanceMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.whiteBalanceCrGain, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.whiteBalanceCbGain, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.analogTVOutputStandard, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.imageStabilizationLevel, 0.0);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.extensionFlag, 0);
					if(deviceInfo->imageConfig.extensionFlag == 1)
					{
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.flickerControl, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.imageStabilizationMode, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.deNoiseMode, 0xFF);
						check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.deNoiseStrength, 0.0);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfig.backLightControlMode, 0xFF);
						check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfig.backLightControlStrength, 0.0);
					}
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_IMG_GetStatusOutput:	//GetStatusReply
				if(bufferLength >=13)
				{
					check = Cmd_StringClear(&deviceInfo->focusStatusInfo.error);

					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->focusStatusInfo.position , 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->focusStatusInfo.moveStatus , 0xFF);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->focusStatusInfo.error);

					User_getStatusReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_IMG_GetOptionsOutput:	//GetOptions Reply
				if(bufferLength >=185)
				{
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->imageConfigOption.backlightCompensationMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.backlightCompensationLevelMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.backlightCompensationLevelMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.brightnessMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.brightnessMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.colorSaturationMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.colorSaturationMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.contrastMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.contrastMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMode, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposurePriority, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.minExposureTimeMin, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.minExposureTimeMax, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.maxExposureTimeMin, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.maxExposureTimeMax, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMinGainMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMinGainMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMaxGainMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMaxGainMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMinIrisMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMinIrisMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMaxIrisMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureMaxIrisMax, 0.0);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureTimeMin, 0xFF);
					check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureTimeMax, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureGainMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureGainMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureIrisMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.exposureIrisMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.autoFocusMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusDefaultSpeedMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusDefaultSpeedMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusNearLimitMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusNearLimitMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusFarLimitMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.focusFarLimitMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.irCutFilterMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.sharpnessMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.sharpnessMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.wideDynamicRangeMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.wideDynamicRangeLevelMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.wideDynamicRangeLevelMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.whiteBalanceMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.whiteBalanceCrGainMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.whiteBalanceCrGainMax, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.whiteBalanceCbGainMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.whiteBalanceCbGainMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.imageStabilizationMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.imageStabilizationLevelMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.imageStabilizationLevelMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.flickerControl, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.analogTVOutputStandard, 0xFF);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.deNoiseMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.deNoiseStrengthMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.deNoiseStrengthMax, 0.0);
					check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.backLightControlMode, 0xFF);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.backLightControlStrengthMin, 0.0);
					check = Cmd_FloatRead(Buffer, checkByte,&index, &deviceInfo->imageConfigOption.backLightControlStrengthMax, 0.0);

					User_getOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetUserDefinedSettingsOutput:
				if(bufferLength >=8)
				{
					check = Cmd_StringClear(&deviceInfo->userDefinedSettings.uerDefinedData);
					check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->userDefinedSettings.uerDefinedData);

					User_getUserDefinedSettingsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//-------------------Imaging Service-------------------------

				//-------------------Media Service--------------------------
			case CMD_GetProfilesOutput:	//GetProfilesReply
				if(bufferLength >=16)
				{
					for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].name);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].token);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName);
						check = Cmd_StringClear(&deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken);
					}
					User_memory_free(deviceInfo->mediaProfiles.mediaProfilesParam);
					deviceInfo->mediaProfiles.mediaProfilesParam = NULL;
					deviceInfo->mediaProfiles.profilesListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.profilesListSize, 0);
					deviceInfo->mediaProfiles.mediaProfilesParam = (MediaProfilesParam*) User_memory_allocate( deviceInfo->mediaProfiles.profilesListSize * sizeof(MediaProfilesParam));
					for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].name);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].token);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcUseCount, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_x, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_y, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsWidth, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsHeight, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateMode, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateDegree, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcMirrorMode, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcUseCount, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncUseCount, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncEncodingMode, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncResolutionWidth, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncResolutionHeight, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncQuality, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlFrameRateLimit, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlEncodingInterval, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlBitrateLimit, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlType, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncGovLength, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncProfile, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncUseCount, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncEncoding, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncBitrate, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncSampleRate, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputUseCount, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputSendPrimacy, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputLevel, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecUseCount, 0xFF);
					}
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->mediaProfiles.extensionFlag, 0);
					if(deviceInfo->mediaProfiles.extensionFlag == 1)
					{
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);

						if( tmpByte!= deviceInfo->mediaProfiles.profilesListSize)
						{
							printf("VideoEncoder Configuration RateControl TargetBitrateLimit List Size = %u Not Equl To Profiles List Size!!!\n", tmpByte);
							if(tmpByte < deviceInfo->mediaProfiles.profilesListSize)
							{
								for( i = 0; i < tmpByte; i ++)
								{
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit, 0xFF);
								}
								for( i = tmpByte; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
								{
									deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit = 0xFF;
								}
							}else
							{
								for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
								{
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit, 0xFF);
								}
							}
						}else
						{
							for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
							{
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit, 0xFF);
							}
						}
					}else
					{
						for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
						{
							deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit = 0xFF;
						}
					}

					User_getProfilesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoSourcesOutput:	//GetVideoSourcesReply
				if(bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->videoSrc.videoSrcListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->videoSrc.srcList[i].token);
					}
					User_memory_free(deviceInfo->videoSrc.srcList);
					deviceInfo->videoSrc.srcList = NULL;
					deviceInfo->videoSrc.videoSrcListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrc.videoSrcListSize, 0);
					deviceInfo->videoSrc.srcList = (VideoSrcParam*) User_memory_allocate( deviceInfo->videoSrc.videoSrcListSize * sizeof(VideoSrcParam));
					for( i = 0; i < deviceInfo->videoSrc.videoSrcListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoSrc.srcList[i].token);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoSrc.srcList[i].frameRate, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrc.srcList[i].resolutionWidth, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrc.srcList[i].resolutionHeight, 0xFF);
					}

					User_getVideoSrcReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoSourceConfigurationsOutput:	//GetVideoSourceConfigurationsReply
				if(bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->videoSrcConfig.configList[i].name);
						check = Cmd_StringClear(&deviceInfo->videoSrcConfig.configList[i].token);
						check = Cmd_StringClear(&deviceInfo->videoSrcConfig.configList[i].srcToken);
					}
					User_memory_free(deviceInfo->videoSrcConfig.configList);
					deviceInfo->videoSrcConfig.configList = NULL;
					deviceInfo->videoSrcConfig.configListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfig.configListSize, 0);
					deviceInfo->videoSrcConfig.configList = (VideoSrcConfigParam*) User_memory_allocate( deviceInfo->videoSrcConfig.configListSize * sizeof(VideoSrcConfigParam));
					for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoSrcConfig.configList[i].name);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoSrcConfig.configList[i].token);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoSrcConfig.configList[i].srcToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfig.configList[i].useCount, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].bounds_x, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].bounds_y, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].boundsWidth, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].boundsHeight, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfig.configList[i].rotateMode, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].rotateDegree, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfig.configList[i].mirrorMode, 0xFF);
					}
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfig.extensionFlag, 0);
					if(deviceInfo->videoSrcConfig.extensionFlag == 1)
					{
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);
						if(tmpByte != deviceInfo->videoSrcConfig.configListSize)
						{
							printf("Max Frame Rate Configuration List Size = %u Not Equal To Configuration List Size!!\n", tmpByte);
							if(tmpByte < deviceInfo->videoSrcConfig.configListSize)
							{
								for( i = 0; i < tmpByte; i ++)
								{
									check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].maxFrameRate, 0xFF);
								}
								for( i = tmpByte; i < deviceInfo->videoSrcConfig.configListSize; i ++)
								{
									deviceInfo->videoSrcConfig.configList[i].maxFrameRate = 0xFF;
								}
							}else
							{
								for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
								{
									check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].maxFrameRate, 0xFF);
								}
							}
						}else
						{
							for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
							{
								check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfig.configList[i].maxFrameRate, 0xFF);
							}
						}
					}else
					{
						for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
						{
							deviceInfo->videoSrcConfig.configList[i].maxFrameRate = 0xFF;
						}
					}

					User_getVideoSrcConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput:	//GetGuaranteedNumberOfVideoEncoderInstancesReply
				if(bufferLength >=13)
				{
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->guaranteedEncs.TotallNum, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->guaranteedEncs.JPEGNum, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->guaranteedEncs.H264Num, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->guaranteedEncs.MPEG4Num, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->guaranteedEncs.MPEG2Num, 0xFF);

					User_getGuaranteedEncsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoEncoderConfigurationsOutput:	//GetVideoEncoderConfigurationsReply
				if(bufferLength >=10)
				{
					for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->videoEncConfig.configList[i].name);
						check = Cmd_StringClear(&deviceInfo->videoEncConfig.configList[i].token);
					}
					User_memory_free(deviceInfo->videoEncConfig.configList);
					deviceInfo->videoEncConfig.configList = NULL;
					deviceInfo->videoEncConfig.configListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.configListSize, 0);
					deviceInfo->videoEncConfig.configList = (VideoEncConfigParam*) User_memory_allocate( deviceInfo->videoEncConfig.configListSize * sizeof(VideoEncConfigParam));
					for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoEncConfig.configList[i].name);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoEncConfig.configList[i].token);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.configList[i].useCount, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.configList[i].encoding, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].width, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].height, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].quality, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].frameRateLimit, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].encodingInterval, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].bitrateLimit, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.configList[i].rateControlType, 0xFF);
						check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].govLength, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.configList[i].profile, 0xFF);
					}
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfig.extensionFlag, 0);
					if(deviceInfo->videoEncConfig.extensionFlag == 1)
					{
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);
						if(tmpByte != deviceInfo->videoEncConfig.configListSize)
						{
							printf("Configuration Extend2 List Size = %u Not Equal To Configuration List Size!!\n", tmpByte);
							if(tmpByte < deviceInfo->videoEncConfig.configListSize)
							{
								for( i = 0; i < tmpByte; i ++)
								{
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].targetBitrateLimit, 0xFF);
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].aspectRatio, 0xFF);
								}
								for( i = tmpByte; i < deviceInfo->videoEncConfig.configListSize; i ++)
								{
									deviceInfo->videoEncConfig.configList[i].targetBitrateLimit = 0xFF;
									deviceInfo->videoEncConfig.configList[i].aspectRatio = 0xFF;
								}
							}else
							{
								for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
								{
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].targetBitrateLimit, 0xFF);
									check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].aspectRatio, 0xFF);
								}
							}
						}else
						{
							for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
							{
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].targetBitrateLimit, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfig.configList[i].aspectRatio, 0xFF);
							}
						}
					}else
					{
						for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
						{
							deviceInfo->videoEncConfig.configList[i].targetBitrateLimit = 0xFF;
							deviceInfo->videoEncConfig.configList[i].aspectRatio = 0xFF;
						}
					}

					User_getVideoEncConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAudioSourcesOutput:	//GetAudioSources
				if(bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->audioSources.audioSourcesListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken);
					}
					User_memory_free(deviceInfo->audioSources.audioSourcesList);
					deviceInfo->audioSources.audioSourcesList = NULL;
					deviceInfo->audioSources.audioSourcesListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioSources.audioSourcesListSize, 0);
					deviceInfo->audioSources.audioSourcesList = (AudioSourcesParam*) User_memory_allocate( deviceInfo->audioSources.audioSourcesListSize * sizeof(AudioSourcesParam));
					for( i = 0; i < deviceInfo->audioSources.audioSourcesListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioSources.audioSourcesList[i].channels, 0xFF);
					}

					User_getAudioSourcesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAudioSourceConfigurationsOutput:	//GetAudioSourceConfigurationsReply
				if(bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->audioSrcConfig.configListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->audioSrcConfig.audioSrcConfigList[i].name);
						check = Cmd_StringClear(&deviceInfo->audioSrcConfig.audioSrcConfigList[i].token);
						check = Cmd_StringClear(&deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken);
					}
					User_memory_free(deviceInfo->audioSrcConfig.audioSrcConfigList);
					deviceInfo->audioSrcConfig.audioSrcConfigList = NULL;
					deviceInfo->audioSrcConfig.configListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioSrcConfig.configListSize, 0);
					deviceInfo->audioSrcConfig.audioSrcConfigList = (AudioSrcConfigParam*) User_memory_allocate( deviceInfo->audioSrcConfig.configListSize * sizeof(AudioSrcConfigParam));
					for( i = 0; i < deviceInfo->audioSrcConfig.configListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].name);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].token);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioSrcConfig.audioSrcConfigList[i].useCount, 0xFF);
					}

					User_getAudioSourceConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAudioEncoderConfigurationsOutput:	//GetAudioEncoderConfigurationsReply
				if(bufferLength >=9)
				{
					for( i = 0; i < deviceInfo->audioEncConfig.configListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->audioEncConfig.configList[i].token);
						check = Cmd_StringClear(&deviceInfo->audioEncConfig.configList[i].name);
					}
					User_memory_free(deviceInfo->audioEncConfig.configList);
					deviceInfo->audioEncConfig.configList = NULL;
					deviceInfo->audioEncConfig.configListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfig.configListSize, 0);
					deviceInfo->audioEncConfig.configList = (AudioEncConfigParam*) User_memory_allocate( deviceInfo->audioEncConfig.configListSize*sizeof(AudioEncConfigParam));
					for( i = 0; i < deviceInfo->audioEncConfig.configListSize; i ++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioEncConfig.configList[i].token);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioEncConfig.configList[i].name);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfig.configList[i].useCount, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfig.configList[i].encoding, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->audioEncConfig.configList[i].bitrate, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->audioEncConfig.configList[i].sampleRate, 0xFF);
					}

					User_getAudioEncConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoSourceConfigurationOptionsOutput:	//GetVideoSourceConfigurationOptionsReply
				if(bufferLength >=31)
				{
					for( i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; i ++)
					{
						check = Cmd_StringClear(&deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i]);
					}
					User_memory_free(deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList);
					deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = NULL;
					deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize = 0;

					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_X_Min, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_X_Max, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Y_Min, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Y_Max, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Width_Min, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Width_Max, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Height_Min, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.boundsRange_Heigh_Max, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize, 0);
					deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = (RCString*) User_memory_allocate( deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize * sizeof(RCString));
					for( i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; i++)
					{
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i]);
					}
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfigOptions.rotateModeOptions, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoSrcConfigOptions.rotateDegreeMinOption, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfigOptions.mirrorModeOptions, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfigOptions.maxFrameRateMin, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoSrcConfigOptions.maxFrameRateMax, 0xFF);

					User_getVideoSrcConfigOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoEncoderConfigurationOptionsOutput:	//GetVideoEncoderConfigurationOptions Reply
				if(bufferLength >=8)
				{
					//----------------------------Modify
					User_memory_free(deviceInfo->videoEncConfigOptions.resolutionsAvailableList);
					deviceInfo->videoEncConfigOptions.resolutionsAvailableList = NULL;
					deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize = 0;
					User_memory_free(deviceInfo->videoEncConfigOptions.aspectRatioList);
					deviceInfo->videoEncConfigOptions.aspectRatioList = NULL;
					deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize = 0;

					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.encodingOption, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize, 0);
					deviceInfo->videoEncConfigOptions.resolutionsAvailableList = (ResolutionsAvailableList*) User_memory_allocate( deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize * sizeof(ResolutionsAvailableList));
					for( i = 0; i < deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize; i ++)
					{
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].width, 0xFF);
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].height, 0xFF);
					}
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.qualityMin, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.qualityMax, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.frameRateMin, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.frameRateMax, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.encodingIntervalMin, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.encodingIntervalMax, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.bitrateRangeMin, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.bitrateRangeMax, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.rateControlTypeOptions, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.govLengthRangeMin, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.govLengthRangeMax, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.profileOptions, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.targetBitrateRangeMin, 0xFF);
					check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.targetBitrateRangeMax, 0xFF);
					check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize, 0);
					deviceInfo->videoEncConfigOptions.aspectRatioList = (Word* ) User_memory_allocate( deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize * sizeof(Word));
					for( i = 0; i < deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize; i ++)
					{
						check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->videoEncConfigOptions.aspectRatioList[i], 0xFF);
					}

					User_getVideoEncConfigOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAudioSourceConfigurationOptionsOutput:	//GetAudioSourceConfigurationOptions Reply
				if(bufferLength >=9)
				{
						for( i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; i++)
						{
							check = Cmd_StringClear(&deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i]);
						}
						User_memory_free(deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList);
						deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = NULL;
						deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize = 0;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize, 0);
						deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = (RCString*) User_memory_allocate( deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize * sizeof(RCString));
						for( i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; i++)
						{
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i]);
						}

						User_getAudioSrcConfigOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetAudioEncoderConfigurationOptionsOutput:	//GetAudioEncoderConfigurationOptions Reply
				if(bufferLength >=9)
				{
						for( i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; i ++)
							User_memory_free(deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList);
						User_memory_free(deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam);
						deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = NULL;
						deviceInfo->audioEncConfigOptions.configListSize = 0;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.configListSize, 0);
						deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = (AudioEncConfigOptionsParam*) User_memory_allocate( deviceInfo->audioEncConfigOptions.configListSize * sizeof(AudioEncConfigOptionsParam));
						for( i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; i ++)
						{
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].encodingOption, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMin, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMax, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize, 0);
							deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList = (Word*) User_memory_allocate( deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize * sizeof(Word));
							for( j = 0; j < deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize; j ++)
							{
								check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList[j], 0xFF);
							}
						}

						User_getAudioEncConfigOptionsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoOSDConfigurationOutput:	//GetVideoOSDConfiguration Reply
				if(bufferLength >=22)
				{
						check = Cmd_StringClear(&deviceInfo->videoOSDConfig.text);

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.dateEnable, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.datePosition, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.dateFormat, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.timeEnable, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.timePosition, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.timeFormat, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.logoEnable, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.logoPosition, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.logooption, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.detailInfoEnable, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.detailInfoPosition, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.detailInfoOption, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.textEnable, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoOSDConfig.textPosition, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->videoOSDConfig.text);

						User_getVideoOSDConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetVideoPrivateAreaOutput:	//GetVideoPrivateArea Reply
				if(bufferLength >=10)
				{
						User_memory_free(deviceInfo->videoPrivateArea.privateAreaPolygon);
						deviceInfo->videoPrivateArea.privateAreaPolygon = NULL;
						deviceInfo->videoPrivateArea.polygonListSize = 0;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoPrivateArea.polygonListSize, 0);
						deviceInfo->videoPrivateArea.privateAreaPolygon = (PrivateAreaPolygon*) User_memory_allocate( deviceInfo->videoPrivateArea.polygonListSize * sizeof(PrivateAreaPolygon));
						for( i = 0; i < deviceInfo->videoPrivateArea.polygonListSize; i ++)
						{
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_x, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_y, 0xFF);
						}
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->videoPrivateArea.privateAreaEnable, 0xFF);

						User_getVideoPrivateAreaReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//-------------------Media Service--------------------------

				//---------------------PTZ Service--------------------------
			case CMD_GetConfigurationsOutput:	//GetConfigurations Reply
				if (bufferLength >=9)
				{
						for( i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
						{
							check = Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].name);
							check = Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].token);
							check = Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
						}
						User_memory_free(deviceInfo->ptzConfig.PTZConfigList);
						deviceInfo->ptzConfig.PTZConfigList = NULL;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigListSize, 0);
						deviceInfo->ptzConfig.PTZConfigList = (PTZConfigParam*) User_memory_allocate( deviceInfo->ptzConfig.PTZConfigListSize *sizeof(PTZConfigParam));
						for( i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
						{
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzConfig.PTZConfigList[i].name);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].useCount, 0xFF);
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzConfig.PTZConfigList[i].token);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].defaultPanSpeed, 0xFF);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].defaultTiltSpeed, 0xFF);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].defaultZoomSpeed, 0xFF);
							check = Cmd_DwordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].defaultTimeout, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].panLimitMin, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].panLimitMax, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].tiltLimitMin, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].tiltLimitMax, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].zoomLimitMin, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].zoomLimitMax, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].eFlipMode, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.PTZConfigList[i].reverseMode, 0xFF);
						}
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzConfig.extensionFlag, 0);
						if( deviceInfo->ptzConfig.extensionFlag == 1)
						{
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);
							if(tmpByte != deviceInfo->ptzConfig.PTZConfigListSize)
							{
								printf("Configuration Extend2 List Size = %u Not Equal To Configuration List Size!!\n", tmpByte);
								if(tmpByte < deviceInfo->ptzConfig.PTZConfigListSize)
								{
									for( i = 0; i < tmpByte; i ++)
									{
										check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
									}
									for( i = tmpByte; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
									{
										deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.stringLength = 0;
										deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.stringData = NULL;
									}
								}else
								{
									for( i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
									{
										check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
									}
								}
							}else
							{
								for( i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
								{
									check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
								}
							}
						}else
						{
							for( i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
							{
								deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.stringLength = 0;
								deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.stringData = NULL;
							}
						}

						User_getPTZConfigReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_PTZ_GetStatusOutput:	//GetStatus Reply
				if(bufferLength >=23)
				{
						check = Cmd_StringClear(&deviceInfo->ptzStatus.error);

						Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.panPosition, 0xFF);
						Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.tiltPosition, 0xFF);
						Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.zoomPosition, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.panTiltMoveStatus, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.zoomMoveStatus, 0xFF);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzStatus.error);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCHour, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCMinute, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCSecond, 0xFF);
						check = Cmd_WordRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCYear, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCMonth, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzStatus.UTCDay, 0xFF);

						User_getPTZStatusReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetPresetsOutput:	//GetPresets Reply
				if(bufferLength >=9)
				{
						for( i = 0 ; i < deviceInfo->ptzPresetsGet.configListSize; i ++)
						{
							check = Cmd_StringClear(&deviceInfo->ptzPresetsGet.configList[i].presetName);
							check = Cmd_StringClear(&deviceInfo->ptzPresetsGet.configList[i].presetToken);
						}
						User_memory_free(deviceInfo->ptzPresetsGet.configList);

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ptzPresetsGet.configListSize, 0);
						deviceInfo->ptzPresetsGet.configList = (PTZPresetsConfig* ) User_memory_allocate( deviceInfo->ptzPresetsGet.configListSize * sizeof(PTZPresetsConfig));
						for( i = 0 ; i < deviceInfo->ptzPresetsGet.configListSize; i ++)
						{
							Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzPresetsGet.configList[i].presetName);
							Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzPresetsGet.configList[i].presetToken);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzPresetsGet.configList[i].panPosition, 0xFF);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzPresetsGet.configList[i].tiltPosition, 0xFF);
							Cmd_ShortRead(Buffer , checkByte, &index,  &deviceInfo->ptzPresetsGet.configList[i].zoomPosition, 0xFF);
						}

						User_getPresetsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_SetPresetOutput:	//SetPreset Reply
				if(bufferLength >=8)
				{
						check = Cmd_StringClear(&deviceInfo->ptzPresetsSet.presetToken_set);
						check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ptzPresetsSet.presetToken_set);

						User_setPresetReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//---------------------PTZ Service--------------------------

				//-----------------Video Analytics Service-------------------
			case CMD_GetSupportedRulesOutput:	//GetSupportedRules Reply
				if(bufferLength >=9)
				{
						User_memory_free(deviceInfo->supportedRules.ruleType);
						deviceInfo->supportedRules.ruleType = NULL;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->supportedRules.supportListSize, 0);
						deviceInfo->supportedRules.ruleType = (Byte*) User_memory_allocate( deviceInfo->supportedRules.supportListSize*sizeof(Byte));
						for( i = 0; i < deviceInfo->supportedRules.supportListSize; i ++)
						{
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->supportedRules.ruleType[i], 0xFF);
						}

						User_getSupportedRulesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;

			case CMD_GetRulesOutput:	//GetRules Reply
				if(bufferLength >= 10)
				{
						for( i = 0; i < deviceInfo->ruleList.ruleListSize; i ++)
						{
							check = Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].ruleName);
							check = Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].ruleToken);
							check = Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].videoSrcToken);

							if(deviceInfo->ruleList.totalRuleList[i].type == 0x10)
							{
								ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								User_memory_free( ptrRule_LineDetector->detectPolygon);
								ptrRule_LineDetector->detectPolygon = NULL;
								User_memory_free( ptrRule_LineDetector);
								deviceInfo->ruleList.totalRuleList[i].type = 0x00;
								deviceInfo->ruleList.totalRuleList[i].rule = NULL;
								ptrRule_LineDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11)
							{
								ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								User_memory_free( ptrRule_FieldDetector->detectPolygon);
								ptrRule_FieldDetector->detectPolygon = NULL;
								User_memory_free( ptrRule_FieldDetector);
								deviceInfo->ruleList.totalRuleList[i].type = 0x00;
								deviceInfo->ruleList.totalRuleList[i].rule = NULL;
								ptrRule_FieldDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12)
							{
								ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								User_memory_free( ptrRule_MotionDetector->detectPolygon);
								ptrRule_MotionDetector->detectPolygon = NULL;
								User_memory_free( ptrRule_MotionDetector);
								deviceInfo->ruleList.totalRuleList[i].type = 0x00;
								deviceInfo->ruleList.totalRuleList[i].rule = NULL;
								ptrRule_MotionDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13)
							{
								ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;
								User_memory_free( ptrRule_Counting->detectPolygon);
								ptrRule_Counting->detectPolygon = NULL;
								User_memory_free( ptrRule_Counting);
								deviceInfo->ruleList.totalRuleList[i].type = 0x00;
								deviceInfo->ruleList.totalRuleList[i].rule = NULL;
								ptrRule_Counting = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14)
							{
								ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_StringClear(&ptrRule_CellMotion->activeCells);
								User_memory_free( ptrRule_CellMotion);
								deviceInfo->ruleList.totalRuleList[i].type = 0x00;
								deviceInfo->ruleList.totalRuleList[i].rule = NULL;
								ptrRule_CellMotion = NULL;
							}

						}
						User_memory_free(deviceInfo->ruleList.totalRuleList);
						deviceInfo->ruleList.totalRuleList = NULL;

						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ruleList.ruleListSize, 0);
						deviceInfo->ruleList.totalRuleList = (TotalRule*) User_memory_allocate( deviceInfo->ruleList.ruleListSize*sizeof(TotalRule));
						for( i = 0; i <  deviceInfo->ruleList.ruleListSize; i ++)
						{
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].ruleName);
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].ruleToken);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ruleList.totalRuleList[i].type, 0x00);

							if(deviceInfo->ruleList.totalRuleList[i].type == 0x10)
							{
								deviceInfo->ruleList.totalRuleList[i].rule = (Rule_LineDetector*) User_memory_allocate(sizeof(Rule_LineDetector));
								ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_LineDetector->direction, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_LineDetector->polygonListSize, 0);
								ptrRule_LineDetector->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_LineDetector->polygonListSize * sizeof(DetectPolygon));
								for(j = 0; j < ptrRule_LineDetector->polygonListSize; j ++)
								{
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_LineDetector->detectPolygon[j].polygon_x, 0xFF);
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_LineDetector->detectPolygon[j].polygon_y, 0xFF);
								}
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_LineDetector->metadataStreamSwitch, 0xFF);

								ptrRule_LineDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11)
							{
								deviceInfo->ruleList.totalRuleList[i].rule = (Rule_FieldDetector*) User_memory_allocate(sizeof(Rule_FieldDetector));
								ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_FieldDetector->polygonListSize, 0);
								ptrRule_FieldDetector->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_FieldDetector->polygonListSize * sizeof(DetectPolygon));
								for(j = 0; j < ptrRule_FieldDetector->polygonListSize; j ++)
								{
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_FieldDetector->detectPolygon[j].polygon_x, 0xFF);
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_FieldDetector->detectPolygon[j].polygon_y, 0xFF);
								}
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_FieldDetector->metadataStreamSwitch, 0xFF);

								ptrRule_FieldDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12)
							{
								deviceInfo->ruleList.totalRuleList[i].rule = (Rule_MotionDetector*) User_memory_allocate(sizeof(Rule_MotionDetector));
								ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_MotionDetector->motionExpression, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_MotionDetector->polygonListSize, 0);
								ptrRule_MotionDetector->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_MotionDetector->polygonListSize * sizeof(DetectPolygon));
								for(j = 0; j < ptrRule_MotionDetector->polygonListSize; j ++)
								{
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_MotionDetector->detectPolygon[j].polygon_x, 0xFF);
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_MotionDetector->detectPolygon[j].polygon_y, 0xFF);
								}
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_MotionDetector->metadataStreamSwitch, 0xFF);

								ptrRule_MotionDetector = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13)
							{
								deviceInfo->ruleList.totalRuleList[i].rule = (Rule_Counting*) User_memory_allocate(sizeof(Rule_Counting));
								ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_DwordRead(Buffer , checkByte, &index, &ptrRule_Counting->reportTimeInterval, 0xFF);
								check = Cmd_DwordRead(Buffer , checkByte, &index, &ptrRule_Counting->resetTimeInterval, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_Counting->direction, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_Counting->polygonListSize, 0);
								ptrRule_Counting->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_Counting->polygonListSize * sizeof(DetectPolygon));
								for(j = 0; j < ptrRule_Counting->polygonListSize; j ++)
								{
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_Counting->detectPolygon[j].polygon_x, 0xFF);
									check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_Counting->detectPolygon[j].polygon_y, 0xFF);
								}
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_Counting->metadataStreamSwitch, 0xFF);

								ptrRule_Counting = NULL;
							}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14)
							{
								deviceInfo->ruleList.totalRuleList[i].rule = (Rule_CellMotion*) User_memory_allocate(sizeof(Rule_CellMotion));
								ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->minCount, 0xFF);
								check = Cmd_DwordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->alarmOnDelay, 0xFF);
								check = Cmd_DwordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->alarmOffDelay, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->activeCellsSize, 0xFF);
								check = Cmd_StringRead(Buffer , checkByte, &index, &ptrRule_CellMotion->activeCells);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_CellMotion->sensitivity, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutBounds_x, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutBounds_y, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutBounds_width, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutBounds_height, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutColumns, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_CellMotion->layoutRows, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index, &ptrRule_CellMotion->metadataStreamSwitch, 0xFF);

								ptrRule_CellMotion = NULL;
							}
						}
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->ruleList.extensionFlag, 0);
						if(deviceInfo->ruleList.extensionFlag == 1)
						{
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);

							if( tmpByte!= deviceInfo->ruleList.ruleListSize)
							{
								printf("Config List2 Size = %u Not Equal To RuleEngine Parameters List Size !!\n", tmpByte);
								if(tmpByte < deviceInfo->ruleList.ruleListSize)
								{
									for( i = 0; i < tmpByte; i ++)
									{
										check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].videoSrcToken);
										check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].threshold, 0xFF);
										check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].motionSensitivity, 0xFF);
									}
									for( i = tmpByte; i < deviceInfo->ruleList.ruleListSize; i ++)
									{
										deviceInfo->ruleList.totalRuleList[i].videoSrcToken.stringLength = 0;
										deviceInfo->ruleList.totalRuleList[i].videoSrcToken.stringData = NULL;
										deviceInfo->ruleList.totalRuleList[i].threshold = 0xFF;
										deviceInfo->ruleList.totalRuleList[i].motionSensitivity = 0xFF;
									}
								}else
								{
									for( i = 0; i < deviceInfo->ruleList.ruleListSize; i ++)
									{
										check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].videoSrcToken);
										check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].threshold, 0xFF);
										check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].motionSensitivity, 0xFF);
									}
								}
							}else
							{
								for( i = 0; i < deviceInfo->ruleList.ruleListSize; i ++)
								{
									check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].videoSrcToken);
									check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].threshold, 0xFF);
									check = Cmd_ByteRead(Buffer , checkByte, &index, &deviceInfo->ruleList.totalRuleList[i].motionSensitivity, 0xFF);
								}
							}
						}else
						{
							for( i = 0; i < deviceInfo->ruleList.ruleListSize; i ++)
							{
								deviceInfo->ruleList.totalRuleList[i].videoSrcToken.stringLength = 0;
								deviceInfo->ruleList.totalRuleList[i].videoSrcToken.stringData = NULL;
								deviceInfo->ruleList.totalRuleList[i].threshold = 0xFF;
								deviceInfo->ruleList.totalRuleList[i].motionSensitivity = 0xFF;
							}
						}

						User_getRulesReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//-----------------Video Analytics Service-------------------

				//--------------------Metadata Stream----------------------
			case CMD_MetadataStreamOutput:	//Metadata Stream
				if(bufferLength >=11)
				{
						check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.version, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->metadataStreamInfo.type, 0xFF);

						if(deviceInfo->metadataStreamInfo.type == 0x01)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Device.textInformation);

							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Device.deviceVendorID, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Device.deviceModelID, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Device.HWVersionCode, 0xFF);
							check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Device.SWVersionCode, 0xFF);
							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Device.textInformation);
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->metadataStreamInfo.metadata_Device.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Device.extensionFlag == 1)
							{
								check = Cmd_DwordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Device.baudRate, 0xFF);
							}

							User_metadataStreamDeviceReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x02)
						{
							for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
								check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
							User_memory_free(deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList);
							deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = NULL;
							deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize = 0;

							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize, 0);
							deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = (Metadata_StreamParam*) User_memory_allocate( deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize * sizeof(Metadata_StreamParam));
							for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
							{
								check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoPID, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoEncodingType, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoResolutionWidth, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoResolutionHeight, 0xFF);
								check = Cmd_ByteRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoFrameRate, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoBitrate, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioPID, 0xFF);
								check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioEncodingType, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioBitrate, 0xFF);
								check = Cmd_WordRead(Buffer, checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioSampleRate, 0xFF);
								check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].PCRPID, 0xFF);
							}
							check = Cmd_ByteRead(Buffer , checkByte, &index,  &deviceInfo->metadataStreamInfo.metadata_Stream.extensionFlag, 0);
							if( deviceInfo->metadataStreamInfo.metadata_Stream.extensionFlag == 1)
							{
								check = Cmd_ByteRead(Buffer , checkByte, &index,  &tmpByte, 0);
								if(tmpByte != deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize)
								{
									printf("Configuration Extend2 List Size = %u Not Equal To Configuration List Size!!\n", tmpByte);
									if(tmpByte < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize)
									{
										for( i = 0; i < tmpByte; i ++)
										{
											check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
										}
										for( i = tmpByte; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
										{
											deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.stringLength = 0;
											deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.stringData = NULL;
										}
									}else
									{
										for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
										{
											check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
										}
									}
								}else
								{
									for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
									{
										check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
									}
								}
							}else
							{
								for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
								{
									deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.stringLength = 0;
									deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.stringData = NULL;
								}
							}

							User_metadataStreamInfoReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x10)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);

							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_DwordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.objectID, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCHour, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCYear, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCDay, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag == 1)
							{
								check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
							}else
							{
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringLength = 0;
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringData = NULL;
							}

							User_metadataStreamLineEventReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x11)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);

							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_DwordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.objectID, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.IsInside, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCHour, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCYear, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCDay, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag == 1)
							{
								check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
							}else
							{
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringLength = 0;
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringData = NULL;
							}

							User_metadataStreamFieldEventReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x12)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);

							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_DwordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.objectID, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCHour, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCYear, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCDay, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag == 1)
							{
								check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
							}else
							{
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringLength = 0;
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringData = NULL;
							}

							User_metadataStreamMotionEventReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x13)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);

							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_DwordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.objectID, 0xFF);
							check = Cmd_DwordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.count, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCHour, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCYear, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCDay, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag == 1)
							{
								check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
							}else
							{
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringLength = 0;
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringData = NULL;
							}

							User_metadataStreamCountingEventReply(deviceInfo, command );
						}else if(deviceInfo->metadataStreamInfo.type == 0x14)
						{
							check = Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);

							check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.IsMotion, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCHour, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond, 0xFF);
							check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCYear, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.UTCDay, 0xFF);
							check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag, 0);
							if(deviceInfo->metadataStreamInfo.metadata_Event.extensionFlag == 1)
							{
								check = Cmd_StringRead(Buffer , checkByte, &index, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
							}else
							{
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringLength = 0;
								deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.stringData = NULL;
							}

							User_metadataStreamCellMotionEventReply(deviceInfo, command );
						}
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
			case CMD_GetMetadataSettingsOutput:	//GetMetadataSettings Reply
				if(bufferLength >=14)
				{
						check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.deviceInfoEnable, 0xFF);
						check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.deviceInfoPeriod, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.streamInfoEnable, 0xFF);
						check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.streamInfoPeriod, 0xFF);
						check = Cmd_ByteRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.motionDetectorEnable, 0xFF);
						check = Cmd_WordRead(Buffer , checkByte,&index, &deviceInfo->metadataSettings.motionDetectorPeriod, 0xFF);

						User_getMetadataSettingsReply(deviceInfo, command );
				}
				else
					error = ReturnChannelError_Reply_WRONG_LENGTH;
				break;
				//--------------------Metadata Stream----------------------
			default :
				error = ReturnChannelError_CMD_NOT_SUPPORTED;
				break;
			}
		}
	}
	return (error);
}

//----------------------General-----------------------

unsigned Cmd_Send(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	switch (command)
	{
	case CMD_GetTxDeviceAddressIDInput:
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetTransmissionParameterCapabilitiesInput:				//GetTransmissionParameterCapabilities
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetTransmissionParametersInput:			//GetTransmissionParameters
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetHwRegisterValuesInput:			//GetHwRegisterValues
		error = Cmd_getHwRegisterValues(deviceInfo, command);
		break;
	case CMD_GetAdvanceOptionsInput:				//GetAdvanceOptions
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetTPSInformationInput:
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetSiPsiTableInput:				//GetSiPsiTable
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetNitLocationInput:				//GetNitLocation
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetSdtServiceInput:				//GetSdtService
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetEITInformationInput:				//GetEITInformation
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->eitInfo.videoEncConfigToken ,command);
		break;
	case CMD_SetTxDeviceAddressIDInput:				//SetTxDeviceAddressID
		error = Cmd_setTxDeviceAddressID(deviceInfo, command);
		break;
	case CMD_SetCalibrationTableInput:				//SetCalibrationTable
		error = Cmd_setCalibrationTable(deviceInfo, command);
		break;
	case CMD_SetTransmissionParametersInput:				//SetTransmissionParameters
		error = Cmd_setTransmissionParameters(deviceInfo, command);
		break;
	case CMD_SetHwRegisterValuesInput:				//SetHwRegisterValues
		error = Cmd_setHwRegisterValues(deviceInfo, command);
		break;
	case CMD_SetAdvaneOptionsInput:				//SetAdvanceOptions
		error = Cmd_setAdvanceOptions(deviceInfo, command);
		break;
	case CMD_SetTPSInformationInput:
		error = Cmd_setTPSInfo(deviceInfo, command);
		break;
	case CMD_SetSiPsiTableInput:			//SetSiPsiTable
		error = Cmd_setPSITable(deviceInfo, command);
		break;
	case CMD_SetNitLocationInput:			//SetNitLocation
		error = Cmd_setNitLocation(deviceInfo, command);
		break;
	case CMD_SetSdtServiceInput:			//SetSdtServiceConfiguration
		error = Cmd_setSdtServiceConfiguration(deviceInfo, command);
		break;
	case CMD_SetEITInformationInput:			//SetEITInformation
		error = Cmd_setEITInformation(deviceInfo, command);
		break;
	case CMD_GetCapabilitiesInput:	//GetCapabilities
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetDeviceInformationInput:	//GetDeviceInformation
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetHostnameInput:	//GetHostname
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetSystemDateAndTimeInput:	//GetSystemDateAndTime
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetSystemLogInput:	//GetSystemLog
		error = Cmd_getWithByte(deviceInfo,  deviceInfo->systemLog.logType, command);
		break;
	case CMD_GetOSDInformationInput:	//GetOSDInformation
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_SystemRebootInput:	//SystemReboot
		error = Cmd_getWithByte(deviceInfo, deviceInfo->systemReboot.rebootType, command);
		break;
	case CMD_SetSystemFactoryDefaultInput:	//SetSystemFactoryDefault
		error = Cmd_getWithByte(deviceInfo, deviceInfo->systemDefault.factoryDefault, command);
		break;
	case CMD_SetHostnameInput:	//SetHostname
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->hostInfo.hostName , command);
		break;
	case CMD_SetSystemDateAndTimeInput:	//SetSystemDateAndTime
		error = Cmd_setSystemDateAndTime(deviceInfo, command);
		break;
	case CMD_SetOSDInformationInput:	//SetOSDInformation
		error = Cmd_setOSDInformation(deviceInfo, command);
		break;
	case CMD_UpgradeSystemFirmwareInput:	//UpgradeSystemFirmware
		error = Cmd_upgradeSystemFirmware(deviceInfo, command);
		break;
	case CMD_GetDigitalInputsInput:	//GetDigitalInputs
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetRelayOutputsInput:	//GetRelayOutputs
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_SetRelayOutputStateInput:	//SetRelayOutputState.
		error = Cmd_setRelayOutputState(deviceInfo, command);
		break;
	case CMD_SetRelayOutputSettingsInput:	//SetRelayOutputSettings
		error = Cmd_setRelayOutputSettings(deviceInfo, command);
		break;
	case CMD_GetImagingSettingsInput:	//GetImagingSettings
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->imageConfig.videoSourceToken , command);
		break;
	case CMD_IMG_GetStatusInput:	//GetStatus
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->focusStatusInfo.videoSourceToken , command);
		break;
	case CMD_IMG_GetOptionsInput:	//GetOptions
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->imageConfigOption.videoSourceToken , command);
		break;
	case CMD_GetUserDefinedSettingsInput:
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->userDefinedSettings.videoSourceToken , command);
		break;
	case CMD_SetImagingSettingsInput:	//SetImagingSettings
		error = Cmd_setImagingSettings(deviceInfo, command);
		break;
	case CMD_IMG_MoveInput:	//Move
		error = Cmd_move(deviceInfo, command);
		break;
	case CMD_IMG_StopInput:	//Stop
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->focusStopInfo.videoSourceToken , command);
		break;
	case CMD_SetUserDefinedSettingsInput:
		error = Cmd_setUserDefinedSettings(deviceInfo, command);
		break;
	case CMD_GetProfilesInput:	//GetProfiles
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetVideoSourcesInput:	//GetVideoSources
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetVideoSourceConfigurationsInput:	//GetVideoSourceConfigurations
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetGuaranteedNumberOfVideoEncoderInstancesInput:	//GetGuaranteedNumberOfVideoEncoderInstances
		error = Cmd_getWithString(deviceInfo, &deviceInfo->guaranteedEncs.configurationToken, command);
		break;
	case CMD_GetVideoEncoderConfigurationsInput:	//GetVideoEncoderConfigurations
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetAudioSourcesInput:	//GetAudioSources
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetAudioSourceConfigurationsInput:	//GetAudioSourceConfigurations
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetAudioEncoderConfigurationsInput:	//GetAudioEncoderConfigurations
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetVideoSourceConfigurationOptionsInput:	//GetVideoSourceConfigurationOptions
		error = Cmd_getVideoSrcConfigOptions(deviceInfo, command);
		break;
	case CMD_GetVideoEncoderConfigurationOptionsInput:	//GetVideoEncoderConfigurationOptions
		error = Cmd_getVideoEncConfigOptions(deviceInfo, command);
		break;
	case CMD_GetAudioSourceConfigurationOptionsInput:	//GetAudioSourceConfigurationOptions
		error = Cmd_getAudioSrcConfigOptions(deviceInfo, command);
		break;
	case CMD_GetAudioEncoderConfigurationOptionsInput:	//GetAudioEncoderConfigurationOptions
		error = Cmd_getAudioEncConfigOptions(deviceInfo, command);
		break;
	case CMD_GetVideoOSDConfigurationInput:	//GetVideoOSDConfiguration
		error = Cmd_getVideoOSDConfig(deviceInfo, command);
		break;
	case CMD_GetVideoPrivateAreaInput:	//GetVideoPrivateArea
		error = Cmd_getVideoPrivateArea(deviceInfo, command);
		break;
	case CMD_SetSynchronizationPointInput:	//SetSynchronizationPoint
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->syncPoint.profileToken ,command);
		break;
	case CMD_SetVideoSourceConfigurationInput:	//SetVideoSourceConfiguration
		error = Cmd_setVideoSrcConfig(deviceInfo, command);
		break;
	case CMD_SetVideoEncoderConfigurationInput:	//SetVideoEncoderConfiguration
		error = Cmd_setVideoEncConfig(deviceInfo, command);
		break;
	case CMD_SetAudioSourceConfigurationInput:	//SetAudioSourceConfiguration
		error = Cmd_setAudioSrcConfig(deviceInfo, command);
		break;
	case CMD_SetAudioEncoderConfigurationInput:	//SetAudioEncoderConfiguration
		error = Cmd_setAudioEncConfig(deviceInfo, command);
		break;
	case CMD_SetVideoOSDConfigurationInput:	//SetVideoOSDConfiguration
		error = Cmd_setVideoOSDConfig(deviceInfo, command);
		break;
	case CMD_SetVideoPrivateAreaInput:	//SetVideoPrivateArea
		error = Cmd_setVideoPrivateArea(deviceInfo, command);
		break;
	case CMD_SetVideoSourceControlInput:
		error = Cmd_setVideoSrcControl(deviceInfo, command);
		break;
	case CMD_GetConfigurationsInput:	//GetConfigurations
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_PTZ_GetStatusInput:	//GetStatus
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->ptzStatus.token , command);
		break;
	case CMD_GetPresetsInput:	//GetPresets
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->ptzPresetsGet.token , command);
		break;
	case CMD_GotoPresetInput:	//GotoPreset
		error = Cmd_gotoPreset(deviceInfo, command);
		break;
	case CMD_RemovePresetInput:	//RemovePreset
		error = Cmd_removePreset(deviceInfo, command);
		break;
	case CMD_SetPresetInput:	//SetPreset
		error = Cmd_setPreset(deviceInfo, command);
		break;
	case CMD_AbsoluteMoveInput:	//AbsoluteMove
		error = Cmd_absoluteMove(deviceInfo, command);
		break;
	case CMD_RelativeMoveInput:	//RelativeMove
		error = Cmd_relativeMove(deviceInfo, command);
		break;
	case CMD_ContinuousMoveInput:	//ContinuousMove
		error = Cmd_continuousMove(deviceInfo, command);
		break;
	case CMD_SetHomePositionInput:	//SetHomePosition
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->ptzHomePosition.token , command);
		break;
	case CMD_GotoHomePositionInput:	//GotoHomePosition
		error = Cmd_gotoHomePosition(deviceInfo, command);
		break;
	case CMD_PTZ_StopInput:	//Stop
		error = Cmd_PTZStop(deviceInfo, command);
		break;
	case CMD_GetSupportedRulesInput:	//GetSupportedRules
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_GetRulesInput:	//GetRules
		error = Cmd_generalGet(deviceInfo, command);
		break;
	case CMD_CreateRuleInput:	//CreateRule
		error = Cmd_createRule(deviceInfo, command);
		break;
	case CMD_ModifyRuleInput:	//ModifyRule
		error = Cmd_modifyRule(deviceInfo, command);
		break;
	case CMD_DeleteRuleInput:	//DeleteRule
		error = Cmd_getWithString(deviceInfo,  &deviceInfo->deleteRule.ruleToken , command);
		break;
	case CMD_GetMetadataSettingsInput:
		error = Cmd_getMetadataSettings(deviceInfo, command);
		break;
	case CMD_SetMetadataSettingsInput:
		error = Cmd_setMetadataSettings(deviceInfo, command);
		break;
		//------------ Metadata Stream Service Service------------
	default:
		error = ReturnChannelError_CMD_NOT_SUPPORTED;
		printf("\nCMD Error : CMD_NOT_SUPPORTED\n");
		break;
	}
	return error;
}

unsigned Cmd_generalGet(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 8;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

    payload_end_point = bufferLength-1;

    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_getWithByte(IN RCHostInfo * deviceInfo, IN Byte byteData, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = byteData;

    payload_end_point = bufferLength-1;

    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_getWithString(IN RCHostInfo * deviceInfo, IN const RCString * string, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 8;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size =  cmd_buffer_size + (4 + string->stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, string, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_getWithStrings(IN RCHostInfo * deviceInfo, IN const RCString * stringArray, IN unsigned stringSize, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 8;
	unsigned cmdsize = 0;
	unsigned i;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	for( i = 0; i < stringSize; i ++)
		cmd_buffer_size =  cmd_buffer_size + (4 + stringArray[i].stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	for( i = 0; i < stringSize; i ++)
		Cmd_StringAssign(cmd_buffer, &stringArray[i], &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

//----------------------General-----------------------

//----------------------ccHDtv Service---------------
unsigned Cmd_getHwRegisterValues(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 14;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const HwRegisterInfo* hwRegisterInfo = &deviceInfo->hwRegisterInfo;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command, &bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = hwRegisterInfo->processor;
	Cmd_DwordAssign(cmd_buffer,hwRegisterInfo->registerAddress ,&bufferLength);
	cmd_buffer[bufferLength++] = hwRegisterInfo->valueListSize;

    payload_end_point = bufferLength-1;

    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setTxDeviceAddressID(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0080;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 11;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const NewTxDevice* newTxDevice = &deviceInfo->newTxDevice;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = newTxDevice->IDType;
	Cmd_WordAssign(cmd_buffer, newTxDevice->deviceAddressID,&bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setCalibrationTable(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0081;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 10;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const CalibrationTable* calibrationTable = &deviceInfo->calibrationTable;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + ( 4 + calibrationTable->tableData.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = calibrationTable->accessOption;
	cmd_buffer[bufferLength++] = calibrationTable->tableType;
	Cmd_StringAssign(cmd_buffer, &calibrationTable->tableData, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setTransmissionParameters(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0082;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 29;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const TransmissionParameter* transmissionParameter = &deviceInfo->transmissionParameter;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = transmissionParameter->bandwidth;
	Cmd_DwordAssign(cmd_buffer,transmissionParameter->frequency,&bufferLength);
	cmd_buffer[bufferLength++] = transmissionParameter->constellation;
	cmd_buffer[bufferLength++] = transmissionParameter->FFT;
	cmd_buffer[bufferLength++] = transmissionParameter->codeRate;
	cmd_buffer[bufferLength++] = transmissionParameter->interval;
	cmd_buffer[bufferLength++] = transmissionParameter->attenuation;
	cmd_buffer[bufferLength++] = transmissionParameter->extensionFlag;
	cmd_buffer[bufferLength++] = transmissionParameter->attenuation_signed;
	Cmd_WordAssign(cmd_buffer,transmissionParameter->TPSCellID,&bufferLength);
	cmd_buffer[bufferLength++] = transmissionParameter->channelNum;
	cmd_buffer[bufferLength++] = transmissionParameter->bandwidthStrapping;
	cmd_buffer[bufferLength++] = transmissionParameter->TVStandard;
	cmd_buffer[bufferLength++] = transmissionParameter->segmentationMode;
	cmd_buffer[bufferLength++] = transmissionParameter->oneSeg_Constellation;
	cmd_buffer[bufferLength++] = transmissionParameter->oneSeg_CodeRate;
	cmd_buffer[bufferLength++] = 0;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setHwRegisterValues(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0083;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	Byte	i;
	unsigned cmd_buffer_size = 14;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const HwRegisterInfo* hwRegisterInfo = &deviceInfo->hwRegisterInfo;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + hwRegisterInfo->valueListSize;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = hwRegisterInfo->processor;
	Cmd_DwordAssign(cmd_buffer,hwRegisterInfo->registerAddress,&bufferLength);
	cmd_buffer[bufferLength++] = hwRegisterInfo->valueListSize;
	for( i = 0; i < hwRegisterInfo->valueListSize; i ++)
		cmd_buffer[bufferLength++] = hwRegisterInfo->registerValues[i];

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setAdvanceOptions(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0084;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 26;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const AdvanceOptions* advanceOptions = &deviceInfo->advanceOptions;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_WordAssign(cmd_buffer, advanceOptions->PTS_PCR_delayTime,&bufferLength);
	Cmd_WordAssign(cmd_buffer, advanceOptions->timeInterval,&bufferLength);
	cmd_buffer[bufferLength++] = advanceOptions->skipNumber;
	cmd_buffer[bufferLength++] = advanceOptions->overFlowNumber;
	Cmd_WordAssign(cmd_buffer, advanceOptions->overFlowSize,&bufferLength);
	cmd_buffer[bufferLength++] = advanceOptions->extensionFlag;
	Cmd_WordAssign(cmd_buffer, advanceOptions->Rx_LatencyRecoverTimeInterval,&bufferLength);
	Cmd_WordAssign(cmd_buffer, advanceOptions->SIPSITableDuration,&bufferLength);
	cmd_buffer[bufferLength++] = advanceOptions->frameRateAdjustment;
	cmd_buffer[bufferLength++] = advanceOptions->repeatPacketMode;
	cmd_buffer[bufferLength++] = advanceOptions->repeatPacketNum;
	cmd_buffer[bufferLength++] = advanceOptions->repeatPacketTimeInterval;
	cmd_buffer[bufferLength++] = advanceOptions->TS_TableDisable;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setTPSInfo(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0085;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 15;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const TPSInfo* tpsInfo = &deviceInfo->tpsInfo;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_WordAssign(cmd_buffer, tpsInfo->cellID,&bufferLength);
	cmd_buffer[bufferLength++] = tpsInfo->highCodeRate;
	cmd_buffer[bufferLength++] = tpsInfo->lowCodeRate;
	cmd_buffer[bufferLength++] = tpsInfo->transmissionMode;
	cmd_buffer[bufferLength++] = tpsInfo->constellation;
	cmd_buffer[bufferLength++] = tpsInfo->interval;


    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setPSITable(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0090;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	Byte	i;
	unsigned cmd_buffer_size = 62;
    unsigned cmdsize = 0;

	 const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PSITable* psiTable = &deviceInfo->psiTable;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_WordAssign(cmd_buffer, psiTable->ONID,&bufferLength);
	Cmd_WordAssign(cmd_buffer, psiTable->NID,&bufferLength);
	Cmd_WordAssign(cmd_buffer, psiTable->TSID,&bufferLength);
	for(i=0;i<32;i++)
		cmd_buffer[bufferLength++] = psiTable->networkName[i];
	cmd_buffer[bufferLength++] = psiTable->extensionFlag;
	Cmd_DwordAssign(cmd_buffer, psiTable->privateDataSpecifier,&bufferLength);
	cmd_buffer[bufferLength++] = psiTable->NITVersion;
	cmd_buffer[bufferLength++] = psiTable->countryID;
	cmd_buffer[bufferLength++] = psiTable->languageID;
	for( i=0; i < 8; i++)
		cmd_buffer[bufferLength++] = 0;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setNitLocation(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0091;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 16;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const NITLoacation* nitLoacation = &deviceInfo->nitLoacation;

	cmd_buffer_size = cmd_buffer_size + ( 4 + security->userName.stringLength) + ( 4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_WordAssign(cmd_buffer, nitLoacation->latitude,&bufferLength);
	Cmd_WordAssign(cmd_buffer, nitLoacation->longitude,&bufferLength);
	Cmd_WordAssign(cmd_buffer, nitLoacation->extentLatitude,&bufferLength);
	Cmd_WordAssign(cmd_buffer, nitLoacation->extentLongitude,&bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setSdtServiceConfiguration(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0092;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 24;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const ServiceConfig* serviceConfig = &deviceInfo->serviceConfig;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + serviceConfig->serviceName.stringLength) + (4 + serviceConfig->provider.stringLength)  ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_WordAssign(cmd_buffer, serviceConfig->serviceID,&bufferLength);
	cmd_buffer[bufferLength++] = serviceConfig->enable;
	Cmd_WordAssign(cmd_buffer, serviceConfig->LCN,&bufferLength);
	Cmd_StringAssign(cmd_buffer, &serviceConfig->serviceName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &serviceConfig->provider, &bufferLength);
	cmd_buffer[bufferLength++] = serviceConfig->extensionFlag;
	cmd_buffer[bufferLength++] = serviceConfig->IDAssignationMode;
	cmd_buffer[bufferLength++] = serviceConfig->ISDBT_RegionID;
	cmd_buffer[bufferLength++] = serviceConfig->ISDBT_BroadcasterRegionID;
	cmd_buffer[bufferLength++] = serviceConfig->ISDBT_RemoteControlKeyID;
	Cmd_WordAssign(cmd_buffer, serviceConfig->ISDBT_ServiceIDDataType_1, &bufferLength);
	Cmd_WordAssign(cmd_buffer, serviceConfig->ISDBT_ServiceIDDataType_2, &bufferLength);
	Cmd_WordAssign(cmd_buffer, serviceConfig->ISDBT_ServiceIDPartialReception, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setEITInformation(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0093;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
   	unsigned cmdsize = 0;
	Byte i = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const EITInfo* eitInfo = &deviceInfo->eitInfo;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + eitInfo->videoEncConfigToken.stringLength);
	for( i = 0; i < eitInfo->listSize; i ++)
	{
		cmd_buffer_size = cmd_buffer_size + 13 + (4 + eitInfo->eitInfoParam[i].eventName.stringLength) + (4 + eitInfo->eitInfoParam[i].eventText.stringLength);
	}

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &eitInfo->videoEncConfigToken, &bufferLength);
	cmd_buffer[bufferLength++] = eitInfo->listSize;
	for( i = 0; i < eitInfo->listSize; i ++)
	{
		cmd_buffer[bufferLength++] = eitInfo->eitInfoParam[i].enable;
		Cmd_DwordAssign(cmd_buffer,eitInfo->eitInfoParam[i].startDate,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,eitInfo->eitInfoParam[i].startTime,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,eitInfo->eitInfoParam[i].duration,&bufferLength);
		Cmd_StringAssign(cmd_buffer, &eitInfo->eitInfoParam[i].eventName, &bufferLength);
		Cmd_StringAssign(cmd_buffer, &eitInfo->eitInfoParam[i].eventText, &bufferLength);
	}

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}
//----------------------ccHDtv Service---------------

//-----------Device Management Service----------
unsigned Cmd_setSystemDateAndTime(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0183;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	Byte i = 0;
	unsigned cmd_buffer_size = 35;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const SystemTime* systemTime = &deviceInfo->systemTime;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	for( i = 0; i < 3; i ++)
		cmd_buffer[bufferLength++] = systemTime->countryCode[i];
	cmd_buffer[bufferLength++] = systemTime->countryRegionID;
	cmd_buffer[bufferLength++] = systemTime->daylightSavings;
	cmd_buffer[bufferLength++] = systemTime->timeZone;
	cmd_buffer[bufferLength++] = systemTime->UTCHour;
	cmd_buffer[bufferLength++] = systemTime->UTCMinute;
	cmd_buffer[bufferLength++] = systemTime->UTCSecond;
	Cmd_WordAssign(cmd_buffer,systemTime->UTCYear,&bufferLength);
	cmd_buffer[bufferLength++] = systemTime->UTCMonth;
	cmd_buffer[bufferLength++] = systemTime->UTCDay;
	cmd_buffer[bufferLength++] = systemTime->extensionFlag;
	Cmd_WordAssign(cmd_buffer,  systemTime->UTCMillisecond,&bufferLength);
	cmd_buffer[bufferLength++] = systemTime->timeAdjustmentMode;
	Cmd_DwordAssign(cmd_buffer,  systemTime->timeAdjustmentCriterionMax,&bufferLength);
	Cmd_DwordAssign(cmd_buffer,  systemTime->timeAdjustmentCriterionMin,&bufferLength);
	Cmd_WordAssign(cmd_buffer,  systemTime->timeSyncDuration,&bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setOSDInformation(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0185;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 22;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const OSDInfo* osdInfo = &deviceInfo->osdInfo;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + osdInfo->text.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = osdInfo->dateEnable;
	cmd_buffer[bufferLength++] = osdInfo->datePosition;
	cmd_buffer[bufferLength++] = osdInfo->dateFormat;
	cmd_buffer[bufferLength++] = osdInfo->timeEnable;
	cmd_buffer[bufferLength++] = osdInfo->timePosition;
	cmd_buffer[bufferLength++] = osdInfo->timeFormat;
	cmd_buffer[bufferLength++] = osdInfo->logoEnable;
	cmd_buffer[bufferLength++] = osdInfo->logoPosition;
	cmd_buffer[bufferLength++] = osdInfo->logooption;
	cmd_buffer[bufferLength++] = osdInfo->detailInfoEnable;
	cmd_buffer[bufferLength++] = osdInfo->detailInfoPosition;
	cmd_buffer[bufferLength++] = osdInfo->detailInfooption;
	cmd_buffer[bufferLength++] = osdInfo->textEnable;
	cmd_buffer[bufferLength++] = osdInfo->textPosition;
	Cmd_StringAssign(cmd_buffer, &osdInfo->text, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_upgradeSystemFirmware(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0186;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
   	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const SystemFirmware* systemFirmware = &deviceInfo->systemFirmware;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + systemFirmware->data.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = systemFirmware->firmwareType;
	Cmd_StringAssign(cmd_buffer, &systemFirmware->data, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}
//-----------Device Management Service----------

//-----------Device IO Service----------

unsigned Cmd_setRelayOutputSettings(IN RCHostInfo * deviceInfo, IN Word command)
{
   unsigned	error = ModulatorError_NO_ERROR;
   //Word    command = 0x0281;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 14;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const RelayOutputsParam* relayOutputsParam = &deviceInfo->relayOutputsSetParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + relayOutputsParam->token.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &relayOutputsParam->token, &bufferLength);
	cmd_buffer[bufferLength++] = relayOutputsParam->mode;
	Cmd_DwordAssign(cmd_buffer,relayOutputsParam->delayTime,&bufferLength);
	cmd_buffer[bufferLength++] = relayOutputsParam->idleState;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setRelayOutputState(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0280;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const RelayOutputState* relayOutputState = &deviceInfo->relayOutputState;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + relayOutputState->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &relayOutputState->token, &bufferLength);
	cmd_buffer[bufferLength++] = relayOutputState->logicalState;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

//-----------Device IO Service----------

//-----------Imaging Service------------

unsigned Cmd_move(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0381;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 28;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const FocusMoveInfo* focusMoveInfo = &deviceInfo->focusMoveInfo;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + focusMoveInfo->videoSourceToken.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &focusMoveInfo->videoSourceToken, &bufferLength);
	Cmd_FloatAssign(cmd_buffer,focusMoveInfo->absolutePosition,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,focusMoveInfo->absoluteSpeed,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,focusMoveInfo->relativeDistance,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,focusMoveInfo->relativeSpeed,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,focusMoveInfo->continuousSpeed,&bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setImagingSettings(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0380;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 122;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const ImageConfig* imageConfig = &deviceInfo->imageConfig;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + imageConfig->videoSourceToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &imageConfig->videoSourceToken, &bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->backlightCompensationMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->backlightCompensationLevel,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->brightness,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->colorSaturation,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->contrast,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->exposureMode;
	cmd_buffer[bufferLength++] = imageConfig->exposurePriority;
	Cmd_WordAssign(cmd_buffer,imageConfig->exposureWindowbottom,&bufferLength);
	Cmd_WordAssign(cmd_buffer,imageConfig->exposureWindowtop,&bufferLength);
	Cmd_WordAssign(cmd_buffer,imageConfig->exposureWindowright,&bufferLength);
	Cmd_WordAssign(cmd_buffer,imageConfig->exposureWindowleft,&bufferLength);
	Cmd_DwordAssign(cmd_buffer,imageConfig->minExposureTime,&bufferLength);
	Cmd_DwordAssign(cmd_buffer,imageConfig->maxExposureTime,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureMinGain,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureMaxGain,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureMinIris,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureMaxIris,&bufferLength);
	Cmd_DwordAssign(cmd_buffer,imageConfig->exposureTime,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureGain,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->exposureIris,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->autoFocusMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->focusDefaultSpeed,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->focusNearLimit,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->focusFarLimit,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->irCutFilterMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->sharpness,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->wideDynamicRangeMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->wideDynamicRangeLevel,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->whiteBalanceMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->whiteBalanceCrGain,&bufferLength);
	Cmd_FloatAssign(cmd_buffer,imageConfig->whiteBalanceCbGain,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->analogTVOutputStandard;
	Cmd_FloatAssign(cmd_buffer,imageConfig->imageStabilizationLevel,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->flickerControl;
	cmd_buffer[bufferLength++] = imageConfig->extensionFlag;
	cmd_buffer[bufferLength++] = imageConfig->forcePersistence;
	cmd_buffer[bufferLength++] = imageConfig->imageStabilizationMode;
	cmd_buffer[bufferLength++] = imageConfig->deNoiseMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->deNoiseStrength,&bufferLength);
	cmd_buffer[bufferLength++] = imageConfig->backLightControlMode;
	Cmd_FloatAssign(cmd_buffer,imageConfig->backLightControlStrength,&bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setUserDefinedSettings(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x03FF;
	const UserDefinedSettings* userDefinedSettings = &deviceInfo->userDefinedSettings;

	RCString RCStringArrayTmp[2];
	RCStringArrayTmp[0] = userDefinedSettings->videoSourceToken;
	RCStringArrayTmp[1] = userDefinedSettings->uerDefinedData;
	error = Cmd_getWithStrings(deviceInfo,RCStringArrayTmp, 2, command);

	return (error);
}
//-----------Imaging Service------------

//-----------Media Service-------------
unsigned Cmd_setVideoSrcControl(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x04C2;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const VideoSrcControl* videoSrcControl = &deviceInfo->videoSrcControl;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + videoSrcControl->videoSrcToken.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &videoSrcControl->videoSrcToken, &bufferLength);
	cmd_buffer[bufferLength++] = videoSrcControl->controlCommand;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setVideoPrivateArea(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x04C1;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 10;
	unsigned cmdsize = 0;
	Byte i = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const VideoPrivateArea* videoPrivateArea = &deviceInfo->videoPrivateArea;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + videoPrivateArea->videoSrcToken.stringLength);
	cmd_buffer_size = cmd_buffer_size + 4 * videoPrivateArea->polygonListSize;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &videoPrivateArea->videoSrcToken, &bufferLength);
	cmd_buffer[bufferLength++] = videoPrivateArea->polygonListSize;
	for( i = 0; i < videoPrivateArea->polygonListSize; i ++)
	{
		Cmd_WordAssign(cmd_buffer, videoPrivateArea->privateAreaPolygon[i].polygon_x,&bufferLength);
		Cmd_WordAssign(cmd_buffer, videoPrivateArea->privateAreaPolygon[i].polygon_y,&bufferLength);
	}
	cmd_buffer[bufferLength++] = videoPrivateArea->privateAreaEnable;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setVideoOSDConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x04C0;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 22;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const VideoOSDConfig* videoOSDConfig = &deviceInfo->videoOSDConfig;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + videoOSDConfig->videoSrcToken.stringLength) + (4 + videoOSDConfig->text.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &videoOSDConfig->videoSrcToken, &bufferLength);
	cmd_buffer[bufferLength++] = videoOSDConfig->dateEnable;
	cmd_buffer[bufferLength++] = videoOSDConfig->datePosition;
	cmd_buffer[bufferLength++] = videoOSDConfig->dateFormat;
	cmd_buffer[bufferLength++] = videoOSDConfig->timeEnable;
	cmd_buffer[bufferLength++] = videoOSDConfig->timePosition;
	cmd_buffer[bufferLength++] = videoOSDConfig->timeFormat;
	cmd_buffer[bufferLength++] = videoOSDConfig->logoEnable;
	cmd_buffer[bufferLength++] = videoOSDConfig->logoPosition;
	cmd_buffer[bufferLength++] = videoOSDConfig->logooption;
	cmd_buffer[bufferLength++] = videoOSDConfig->detailInfoEnable;
	cmd_buffer[bufferLength++] = videoOSDConfig->detailInfoPosition;
	cmd_buffer[bufferLength++] = videoOSDConfig->detailInfoOption;
	cmd_buffer[bufferLength++] = videoOSDConfig->textEnable;
	cmd_buffer[bufferLength++] = videoOSDConfig->textPosition;
	Cmd_StringAssign(cmd_buffer, &videoOSDConfig->text, &bufferLength);

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setAudioEncConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0487;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 15;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const AudioEncConfigParam* audioEncConfigParam = &deviceInfo->audioEncConfigSetParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + audioEncConfigParam->name.stringLength) + (4 + audioEncConfigParam->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &audioEncConfigParam->name, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &audioEncConfigParam->token, &bufferLength);
	cmd_buffer[bufferLength++] = audioEncConfigParam->useCount;
	cmd_buffer[bufferLength++] = audioEncConfigParam->encoding;
	Cmd_WordAssign(cmd_buffer,audioEncConfigParam->bitrate, &bufferLength);
	Cmd_WordAssign(cmd_buffer,audioEncConfigParam->sampleRate, &bufferLength);
	cmd_buffer[bufferLength++] = audioEncConfigParam->forcePersistence;

    payload_end_point = bufferLength-1;
    cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setAudioSrcConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0486;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 10;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const AudioSrcConfigParam* audioSrcConfig = &deviceInfo->audioSrcConfigSetParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + audioSrcConfig->name.stringLength) + (4 + audioSrcConfig->token.stringLength) + (4 + audioSrcConfig->sourceToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &audioSrcConfig->name, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &audioSrcConfig->token, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &audioSrcConfig->sourceToken, &bufferLength);
	cmd_buffer[bufferLength++] = audioSrcConfig->useCount;
	cmd_buffer[bufferLength++] = audioSrcConfig->forcePersistence;

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setVideoEncConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0484;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 28;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const VideoEncConfigParam* videoEncConfigParam = &deviceInfo->videoEncConfigSetParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + videoEncConfigParam->name.stringLength) + (4 + videoEncConfigParam->token.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &videoEncConfigParam->name, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &videoEncConfigParam->token, &bufferLength);
	cmd_buffer[bufferLength++] = videoEncConfigParam->useCount;
	cmd_buffer[bufferLength++] = videoEncConfigParam->encoding;
	Cmd_WordAssign(cmd_buffer,videoEncConfigParam->width, &bufferLength);
	Cmd_WordAssign(cmd_buffer,videoEncConfigParam->height, &bufferLength);
	cmd_buffer[bufferLength++] = videoEncConfigParam->quality;
	cmd_buffer[bufferLength++] = videoEncConfigParam->frameRateLimit;
	cmd_buffer[bufferLength++] = videoEncConfigParam->encodingInterval;
	Cmd_WordAssign(cmd_buffer,videoEncConfigParam->bitrateLimit, &bufferLength);
	cmd_buffer[bufferLength++] = videoEncConfigParam->rateControlType;
	cmd_buffer[bufferLength++] = videoEncConfigParam->govLength;
	cmd_buffer[bufferLength++] = videoEncConfigParam->profile;
	cmd_buffer[bufferLength++] = videoEncConfigParam->forcePersistence;
	cmd_buffer[bufferLength++] = videoEncConfigParam->extensionFlag;
	Cmd_WordAssign(cmd_buffer,videoEncConfigParam->targetBitrateLimit, &bufferLength);
	Cmd_WordAssign(cmd_buffer,videoEncConfigParam->aspectRatio, &bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setVideoSrcConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0482;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 24;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const VideoSrcConfigParam* videoSrcConfigParam = &deviceInfo->videoSrcConfigSetParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + videoSrcConfigParam->name.stringLength) + (4 + videoSrcConfigParam->token.stringLength) + (4 + videoSrcConfigParam->srcToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &videoSrcConfigParam->name, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &videoSrcConfigParam->token, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &videoSrcConfigParam->srcToken, &bufferLength);
	cmd_buffer[bufferLength++] = videoSrcConfigParam->useCount;
	Cmd_WordAssign(cmd_buffer, videoSrcConfigParam->bounds_x, &bufferLength);
	Cmd_WordAssign(cmd_buffer, videoSrcConfigParam->bounds_y, &bufferLength);
	Cmd_WordAssign(cmd_buffer, videoSrcConfigParam->boundsWidth, &bufferLength);
	Cmd_WordAssign(cmd_buffer, videoSrcConfigParam->boundsHeight, &bufferLength);
	cmd_buffer[bufferLength++] = videoSrcConfigParam->rotateMode;
	Cmd_WordAssign(cmd_buffer, videoSrcConfigParam->rotateDegree, &bufferLength);
	cmd_buffer[bufferLength++] = videoSrcConfigParam->mirrorMode;
	cmd_buffer[bufferLength++] = videoSrcConfigParam->forcePersistence;
	cmd_buffer[bufferLength++] = videoSrcConfigParam->extensionFlag;
	cmd_buffer[bufferLength++] = videoSrcConfigParam->maxFrameRate;

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_getVideoSrcConfigOptions(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0408;
	const VideoSrcConfigOptions* videoSrcConfigOptions = &deviceInfo->videoSrcConfigOptions;

	RCString RCStringArrayTmp[2];
	RCStringArrayTmp[0] = videoSrcConfigOptions->videoSrcConfigToken;
	RCStringArrayTmp[1] = videoSrcConfigOptions->profileToken;
	error = Cmd_getWithStrings(deviceInfo, RCStringArrayTmp, 2, command);

	return (error);
}

unsigned Cmd_getVideoEncConfigOptions(IN RCHostInfo * deviceInfo, IN Word command)
{
	unsigned	error = ModulatorError_NO_ERROR;
	//Word    command = 0x0409;
	const VideoEncConfigOptions* videoEncConfigOptions = &deviceInfo->videoEncConfigOptions;

	RCString RCStringArrayTmp[2];
	RCStringArrayTmp[0] = videoEncConfigOptions->videoEncConfigToken;
	RCStringArrayTmp[1] = videoEncConfigOptions->profileToken;
	error = Cmd_getWithStrings(deviceInfo, RCStringArrayTmp, 2, command);

	return (error);
}

unsigned Cmd_getAudioSrcConfigOptions(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x040A;
	const AudioSrcConfigOptions* audioSrcConfigOptions = &deviceInfo->audioSrcConfigOptions;

   RCString RCStringArrayTmp[2];
   RCStringArrayTmp[0] = audioSrcConfigOptions->audioSrcConfigToken;
   RCStringArrayTmp[1] = audioSrcConfigOptions->profileToken;
   error = Cmd_getWithStrings(deviceInfo, RCStringArrayTmp, 2, command);

	return (error);
}

unsigned Cmd_getAudioEncConfigOptions(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x040B;
	const AudioEncConfigOptions* audioEncConfigOptions = &deviceInfo->audioEncConfigOptions;

   RCString RCStringArrayTmp[2];
   RCStringArrayTmp[0] = audioEncConfigOptions->audioEncConfigToken;
   RCStringArrayTmp[1] = audioEncConfigOptions->profileToken;
   error = Cmd_getWithStrings(deviceInfo, RCStringArrayTmp, 2, command);

	return (error);
}

unsigned Cmd_getVideoOSDConfig(IN RCHostInfo * deviceInfo, IN Word command)
{
    //Word    command = 0x0440;
	const VideoOSDConfig* videoOSDConfig = &deviceInfo->videoOSDConfig;

    return Cmd_getWithString(deviceInfo,  &videoOSDConfig->videoSrcToken ,command);
}

unsigned Cmd_getVideoPrivateArea(IN RCHostInfo * deviceInfo, IN Word command)
{
    //Word    command = 0x0441;
    const VideoPrivateArea* videoPrivateArea = &deviceInfo->videoPrivateArea;

    return Cmd_getWithString(deviceInfo,  &videoPrivateArea->videoSrcToken ,command);
}
//-----------Media Service------------

//----------- PTZ  Service------------
unsigned Cmd_gotoPreset(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0580;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 14;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZGotoParam* ptzGotoParam = &deviceInfo->ptzGotoParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzGotoParam->token.stringLength) + (4 + ptzGotoParam->presetToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzGotoParam->token, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &ptzGotoParam->presetToken, &bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzGotoParam->panSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzGotoParam->tiltSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzGotoParam->zoomSpeed,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_removePreset(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0581;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 8;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZRemoveParam* ptzRemoveParam = &deviceInfo->ptzRemoveParam;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzRemoveParam->token.stringLength) + (4 + ptzRemoveParam->presetToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzRemoveParam->token, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &ptzRemoveParam->presetToken, &bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_setPreset(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0582;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 8;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZPresetsSet* ptzPresetsSet = &deviceInfo->ptzPresetsSet;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzPresetsSet->token.stringLength) + (4 + ptzPresetsSet->presetName.stringLength) + (4 + ptzPresetsSet->presetToken.stringLength) ;

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzPresetsSet->token, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &ptzPresetsSet->presetName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &ptzPresetsSet->presetToken, &bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_absoluteMove(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0583;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 20;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZAbsoluteMove* ptzAbsoluteMove = &deviceInfo->ptzAbsoluteMove;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzAbsoluteMove->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzAbsoluteMove->token, &bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->panPosition,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->tiltPosition,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->zoomPosition,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->panSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->tiltSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzAbsoluteMove->zoomSpeed,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_relativeMove(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0584;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 20;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZRelativeMove* ptzRelativeMove = &deviceInfo->ptzRelativeMove;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzRelativeMove->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzRelativeMove->token, &bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->panTranslation,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->tiltTranslation,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->zoomTranslation,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->panSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->tiltSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzRelativeMove->zoomSpeed,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_continuousMove(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0585;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 18;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZContinuousMove* ptzContinuousMove = &deviceInfo->ptzContinuousMove;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzContinuousMove->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzContinuousMove->token, &bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzContinuousMove->panVelocity,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzContinuousMove->tiltVelocity,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzContinuousMove->zoomVelocity,&bufferLength);
	Cmd_DwordAssign(cmd_buffer,ptzContinuousMove->timeout,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_gotoHomePosition(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0587;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 14;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZHomePosition* ptzHomePosition = &deviceInfo->ptzHomePosition;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzHomePosition->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzHomePosition->token, &bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzHomePosition->panSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzHomePosition->tiltSpeed,&bufferLength);
	Cmd_ShortAssign(cmd_buffer, ptzHomePosition->zoomSpeed,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_PTZStop(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0588;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 9;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const PTZStop* ptzStop = &deviceInfo->ptzStop;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + ptzStop->token.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &ptzStop->token, &bufferLength);
	cmd_buffer[bufferLength++] = ptzStop->panTiltZoom;

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}
//----------- PTZ  Service------------

//----------- Video Analytics  Service------------
unsigned Cmd_createRule(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0680;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 12;
	unsigned cmdsize = 0;
	Byte i;
	Rule_LineDetector* ptrRule_LineDetector = NULL;
	Rule_FieldDetector* ptrRule_FieldDetector = NULL;
	Rule_MotionDetector* ptrRule_MotionDetector = NULL;
	Rule_Counting* ptrRule_Counting = NULL;
	Rule_CellMotion* ptrRule_CellMotion = NULL;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const TotalRule* totalRule = &deviceInfo->totalRule;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + totalRule->ruleName.stringLength) + (4 + totalRule->ruleToken.stringLength) + (4 + totalRule->videoSrcToken.stringLength) ;

	if(totalRule->type == 0x10)
	{
		ptrRule_LineDetector = (Rule_LineDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 4 + ( ptrRule_LineDetector->polygonListSize * 4 ) ;
		ptrRule_LineDetector = NULL;
	}else if(totalRule->type == 0x11)
	{
		ptrRule_FieldDetector = (Rule_FieldDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 2 + ( ptrRule_FieldDetector->polygonListSize * 4 ) ;
		ptrRule_FieldDetector = NULL;
	}else if(totalRule->type == 0x12)
	{
		ptrRule_MotionDetector = (Rule_MotionDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 3 + ( ptrRule_MotionDetector->polygonListSize * 4 ) ;
		ptrRule_MotionDetector = NULL;
	}else if(totalRule->type == 0x13)
	{
		ptrRule_Counting = (Rule_Counting*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 12 + ( ptrRule_Counting->polygonListSize * 4 ) ;
		ptrRule_Counting = NULL;
	}else if(totalRule->type == 0x14)
	{
		ptrRule_CellMotion = (Rule_CellMotion*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 24 + ( 4 + ptrRule_CellMotion->activeCells.stringLength ) ;
		ptrRule_CellMotion = NULL;
	}else
	{
		error = ReturnChannelError_CMD_CONTENT_ERROR;
		return error;
	}

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &totalRule->ruleName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &totalRule->ruleToken, &bufferLength);
	cmd_buffer[bufferLength++] = totalRule->type;

	if(totalRule->type == 0x10)
	{
		ptrRule_LineDetector = (Rule_LineDetector*) totalRule->rule;
		Cmd_WordAssign(cmd_buffer, ptrRule_LineDetector->direction, &bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_LineDetector->polygonListSize;
		for( i = 0; i < ptrRule_LineDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_LineDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_LineDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_LineDetector->metadataStreamSwitch;
		ptrRule_LineDetector = NULL;
	}else if(totalRule->type == 0x11)
	{
		ptrRule_FieldDetector = (Rule_FieldDetector*) totalRule->rule;
		cmd_buffer[bufferLength++] = ptrRule_FieldDetector->polygonListSize;
		for( i = 0; i < ptrRule_FieldDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_FieldDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_FieldDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_FieldDetector->metadataStreamSwitch;
		ptrRule_FieldDetector = NULL;
	}else if(totalRule->type == 0x12)
	{
		ptrRule_MotionDetector = (Rule_MotionDetector*) totalRule->rule;
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->motionExpression;
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->polygonListSize;
		for( i = 0; i < ptrRule_MotionDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_MotionDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_MotionDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->metadataStreamSwitch;
		ptrRule_MotionDetector = NULL;
	}else if(totalRule->type == 0x13)
	{
		ptrRule_Counting = (Rule_Counting*) totalRule->rule;
		Cmd_DwordAssign(cmd_buffer, ptrRule_Counting->reportTimeInterval,&bufferLength);
		Cmd_DwordAssign(cmd_buffer, ptrRule_Counting->resetTimeInterval,&bufferLength);
		Cmd_WordAssign(cmd_buffer, ptrRule_Counting->direction,&bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_Counting->polygonListSize;
		for( i = 0; i < ptrRule_Counting->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_Counting->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_Counting->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_Counting->metadataStreamSwitch;
		ptrRule_Counting = NULL;
	}else if(totalRule->type == 0x14)
	{
		ptrRule_CellMotion = (Rule_CellMotion*) totalRule->rule;
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->minCount,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,  ptrRule_CellMotion->alarmOnDelay,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,  ptrRule_CellMotion->alarmOffDelay,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->activeCellsSize,&bufferLength);
		Cmd_StringAssign(cmd_buffer, &ptrRule_CellMotion->activeCells, &bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->sensitivity;
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_x,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_y,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_width,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_height,&bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->layoutColumns;
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->layoutRows;
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->metadataStreamSwitch;
		ptrRule_CellMotion = NULL;
	}

	cmd_buffer[bufferLength++] = totalRule->extensionFlag;
	Cmd_StringAssign(cmd_buffer, &totalRule->videoSrcToken, &bufferLength);
	cmd_buffer[bufferLength++] = totalRule->threshold;
	cmd_buffer[bufferLength++] = totalRule->motionSensitivity;

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

unsigned Cmd_modifyRule(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0x0681;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 12;
	unsigned cmdsize = 0;
	Byte i;
	Rule_LineDetector* ptrRule_LineDetector = NULL;
	Rule_FieldDetector* ptrRule_FieldDetector = NULL;
	Rule_MotionDetector* ptrRule_MotionDetector = NULL;
	Rule_Counting* ptrRule_Counting = NULL;
	Rule_CellMotion* ptrRule_CellMotion = NULL;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const TotalRule* totalRule = &deviceInfo->totalRule;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);
	cmd_buffer_size = cmd_buffer_size + (4 + totalRule->ruleName.stringLength) + (4 + totalRule->ruleToken.stringLength) + (4 + totalRule->videoSrcToken.stringLength) ;

	if(totalRule->type == 0x10)
	{
		ptrRule_LineDetector = (Rule_LineDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 4 + ( ptrRule_LineDetector->polygonListSize * 4 ) ;
		ptrRule_LineDetector = NULL;
	}else if(totalRule->type == 0x11)
	{
		ptrRule_FieldDetector = (Rule_FieldDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 2 + ( ptrRule_FieldDetector->polygonListSize * 4 ) ;
		ptrRule_FieldDetector = NULL;
	}else if(totalRule->type == 0x12)
	{
		ptrRule_MotionDetector = (Rule_MotionDetector*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 3 + ( ptrRule_MotionDetector->polygonListSize * 4 ) ;
		ptrRule_MotionDetector = NULL;
	}else if(totalRule->type == 0x13)
	{
		ptrRule_Counting = (Rule_Counting*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 12 + ( ptrRule_Counting->polygonListSize * 4 ) ;
		ptrRule_Counting = NULL;
	}else if(totalRule->type == 0x14)
	{
		ptrRule_CellMotion = (Rule_CellMotion*) totalRule->rule;
		cmd_buffer_size = cmd_buffer_size + 24 + ( 4 + ptrRule_CellMotion->activeCells.stringLength ) ;
		ptrRule_CellMotion = NULL;
	}else
	{
		error = ReturnChannelError_CMD_CONTENT_ERROR;
		return error;
	}

	std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	Cmd_StringAssign(cmd_buffer, &totalRule->ruleName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &totalRule->ruleToken, &bufferLength);
	cmd_buffer[bufferLength++] = totalRule->type;

	if(totalRule->type == 0x10)
	{
		ptrRule_LineDetector = (Rule_LineDetector*) totalRule->rule;
		Cmd_WordAssign(cmd_buffer, ptrRule_LineDetector->direction, &bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_LineDetector->polygonListSize;
		for( i = 0; i < ptrRule_LineDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_LineDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_LineDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_LineDetector->metadataStreamSwitch;
		ptrRule_LineDetector = NULL;
	}else if(totalRule->type == 0x11)
	{
		ptrRule_FieldDetector = (Rule_FieldDetector*) totalRule->rule;
		cmd_buffer[bufferLength++] = ptrRule_FieldDetector->polygonListSize;
		for( i = 0; i < ptrRule_FieldDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_FieldDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_FieldDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_FieldDetector->metadataStreamSwitch;
		ptrRule_FieldDetector = NULL;
	}else if(totalRule->type == 0x12)
	{
		ptrRule_MotionDetector = (Rule_MotionDetector*) totalRule->rule;
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->motionExpression;
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->polygonListSize;
		for( i = 0; i < ptrRule_MotionDetector->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_MotionDetector->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_MotionDetector->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_MotionDetector->metadataStreamSwitch;
		ptrRule_MotionDetector = NULL;
	}else if(totalRule->type == 0x13)
	{
		ptrRule_Counting = (Rule_Counting*) totalRule->rule;
		Cmd_DwordAssign(cmd_buffer, ptrRule_Counting->reportTimeInterval,&bufferLength);
		Cmd_DwordAssign(cmd_buffer, ptrRule_Counting->resetTimeInterval,&bufferLength);
		Cmd_WordAssign(cmd_buffer, ptrRule_Counting->direction,&bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_Counting->polygonListSize;
		for( i = 0; i < ptrRule_Counting->polygonListSize; i ++)
		{
			Cmd_WordAssign(cmd_buffer,  ptrRule_Counting->detectPolygon[i].polygon_x,&bufferLength);
			Cmd_WordAssign(cmd_buffer,  ptrRule_Counting->detectPolygon[i].polygon_y,&bufferLength);
		}
		cmd_buffer[bufferLength++] = ptrRule_Counting->metadataStreamSwitch;
		ptrRule_Counting = NULL;
	}else if(totalRule->type == 0x14)
	{
		ptrRule_CellMotion = (Rule_CellMotion*) totalRule->rule;
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->minCount,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,  ptrRule_CellMotion->alarmOnDelay,&bufferLength);
		Cmd_DwordAssign(cmd_buffer,  ptrRule_CellMotion->alarmOffDelay,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->activeCellsSize,&bufferLength);
		Cmd_StringAssign(cmd_buffer, &ptrRule_CellMotion->activeCells, &bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->sensitivity;
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_x,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_y,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_width,&bufferLength);
		Cmd_WordAssign(cmd_buffer,  ptrRule_CellMotion->layoutBounds_height,&bufferLength);
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->layoutColumns;
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->layoutRows;
		cmd_buffer[bufferLength++] = ptrRule_CellMotion->metadataStreamSwitch;
		ptrRule_CellMotion = NULL;
	}

	cmd_buffer[bufferLength++] = totalRule->extensionFlag;
	Cmd_StringAssign(cmd_buffer, &totalRule->videoSrcToken, &bufferLength);
	cmd_buffer[bufferLength++] = totalRule->threshold;
	cmd_buffer[bufferLength++] = totalRule->motionSensitivity;

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}
//----------- Video Analytics  Service------------

//----------- Metadata Stream Service------------
unsigned Cmd_getMetadataSettings(IN RCHostInfo * deviceInfo, IN Word command)
{
    //Word    command = 0xE001;
    return Cmd_generalGet(deviceInfo, command);
}

unsigned Cmd_setMetadataSettings(IN RCHostInfo * deviceInfo, IN Word command)
{
    unsigned	error = ModulatorError_NO_ERROR;
    //Word    command = 0xE081;
	unsigned	payload_start_point,payload_end_point;
	unsigned	bufferLength = 0;
	int		txLen;
	unsigned cmd_buffer_size = 17;
	unsigned cmdsize = 0;

	const Security* security = &deviceInfo->security;
	Device* device = &deviceInfo->device;
	CmdSendConfig* cmdSendConfig = &deviceInfo->cmdSendConfig;
	const MetadataSettings* metadataSettings = &deviceInfo->metadataSettings;

	cmd_buffer_size = cmd_buffer_size + (4 + security->userName.stringLength) + (4 + security->password.stringLength);

    std::vector<Byte> cmdBuf(cmd_buffer_size);
    Byte * cmd_buffer = &cmdBuf[0];
	cmdsize = cmd_buffer_size - 1;

	payload_start_point = bufferLength;
	Cmd_DwordAssign(cmd_buffer,cmdsize,&bufferLength);
	Cmd_WordAssign(cmd_buffer, command,&bufferLength);
	cmd_buffer[bufferLength++] = 0xFF;
	Cmd_StringAssign(cmd_buffer, &security->userName, &bufferLength);
	Cmd_StringAssign(cmd_buffer, &security->password, &bufferLength);

	cmd_buffer[bufferLength++] = metadataSettings->deviceInfoEnable;
	Cmd_WordAssign(cmd_buffer, metadataSettings->deviceInfoPeriod,&bufferLength);
	cmd_buffer[bufferLength++] = metadataSettings->streamInfoEnable;
	Cmd_WordAssign(cmd_buffer, metadataSettings->streamInfoPeriod,&bufferLength);
	cmd_buffer[bufferLength++] = metadataSettings->motionDetectorEnable;
	Cmd_WordAssign(cmd_buffer, metadataSettings->motionDetectorPeriod,&bufferLength);

	payload_end_point = bufferLength-1;
	cmd_buffer[bufferLength++] = Cmd_calChecksum(payload_start_point,payload_end_point,cmd_buffer);

	if(deviceInfo->device.TSMode)
		error = Cmd_sendTSCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);
	else
		error = Cmd_sendRCCmd(cmd_buffer, bufferLength, device, &txLen, cmdSendConfig);

	return (error);
}

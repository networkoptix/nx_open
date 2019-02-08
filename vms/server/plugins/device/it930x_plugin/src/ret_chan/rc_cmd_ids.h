#ifndef RETURN_CHANNEL_CMD_IDS_H
#define RETURN_CHANNEL_CMD_IDS_H

#define RETURN_CHANNEL_PID						0x201
#define RETURN_CHANNEL_API_VERSION				0x0013		// Version 0.13

//=============================================================================
//					ccHDtv Service Definition
//=============================================================================
#define CMD_GetTxDeviceAddressIDInput                               0x0000
#define CMD_GetTxDeviceAddressIDOutput                              0x8000
#define CMD_GetTransmissionParameterCapabilitiesInput               0x0001
#define CMD_GetTransmissionParameterCapabilitiesOutput              0x8001
#define CMD_GetTransmissionParametersInput                          0x0002
#define CMD_GetTransmissionParametersOutput                         0x8002
#define CMD_GetHwRegisterValuesInput                                0x0003
#define CMD_GetHwRegisterValuesOutput                               0x8003
#define CMD_GetAdvanceOptionsInput                                  0x0004
#define CMD_GetAdvanceOptionsOutput                                 0x8004
#define CMD_GetTPSInformationInput                                  0x0005
#define CMD_GetTPSInformationOutput                                 0x8005
#define CMD_GetSiPsiTableInput                                      0x0010
#define CMD_GetSiPsiTableOutput                                     0x8010
#define CMD_GetNitLocationInput                                     0x0011
#define CMD_GetNitLocationOutput                                    0x8011
#define CMD_GetSdtServiceInput                                      0x0012
#define CMD_GetSdtServiceOutput                                     0x8012
#define CMD_GetEITInformationInput                                  0x0013
#define CMD_GetEITInformationOutput                                 0x8013
#define CMD_SetTxDeviceAddressIDInput                               0x0080
#define CMD_SetTxDeviceAddressIDOutput                              0x8080
#define CMD_SetCalibrationTableInput                                0x0081
#define CMD_SetCalibrationTableOutput                               0x8081
#define CMD_SetTransmissionParametersInput                          0x0082
#define CMD_SetTransmissionParametersOutput                         0x8082
#define CMD_SetHwRegisterValuesInput                                0x0083
#define CMD_SetHwRegisterValuesOutput                               0x8083
#define CMD_SetAdvaneOptionsInput                                   0x0084
#define CMD_SetAdvaneOptionsOutput                                  0x8084
#define CMD_SetTPSInformationInput                                  0x0085
#define CMD_SetTPSInformationOutput                                 0x8085
#define CMD_SetSiPsiTableInput                                      0x0090
#define CMD_SetSiPsiTableOutput                                     0x8090
#define CMD_SetNitLocationInput                                     0x0091
#define CMD_SetNitLocationOutput                                    0x8091
#define CMD_SetSdtServiceInput                                      0x0092
#define CMD_SetSdtServiceOutput                                     0x8092
#define CMD_SetEITInformationInput                                  0x0093
#define CMD_SetEITInformationOutput                                 0x8093
//=============================================================================
//						Device Management Service Definition
//=============================================================================
#define CMD_GetCapabilitiesInput                                    0x0100
#define CMD_GetCapabilitiesOutput                                   0x8100
#define CMD_GetDeviceInformationInput                               0x0101
#define CMD_GetDeviceInformationOutput                              0x8101
#define CMD_GetHostnameInput                                        0x0102
#define CMD_GetHostnameOutput                                       0x8102
#define CMD_GetSystemDateAndTimeInput                               0x0103
#define CMD_GetSystemDateAndTimeOutput                              0x8103
#define CMD_GetSystemLogInput                                       0x0104
#define CMD_GetSystemLogOutput                                      0x8104
#define CMD_GetOSDInformationInput                                  0x0105
#define CMD_GetOSDInformationOutput                                 0x8105
#define CMD_SystemRebootInput                                       0x0180
#define CMD_SystemRebootOutput                                      0x8180
#define CMD_SetSystemFactoryDefaultInput                            0x0181
#define CMD_SetSystemFactoryDefaultOutput                           0x8181
#define CMD_SetHostnameInput                                        0x0182
#define CMD_SetHostnameOutput                                       0x8182
#define CMD_SetSystemDateAndTimeInput                               0x0183
#define CMD_SetSystemDateAndTimeOutput                              0x8183
#define CMD_SetOSDInformationInput                                  0x0185
#define CMD_SetOSDInformationOutput                                 0x8185
#define CMD_UpgradeSystemFirmwareInput                              0x0186
#define CMD_UpgradeSystemFirmwareOutput                             0x8186
//=============================================================================
//						Device_IO Service Definition
//=============================================================================
#define CMD_GetDigitalInputsInput                                   0x0200
#define CMD_GetDigitalInputsOutput                                  0x8200
#define CMD_GetRelayOutputsInput                                    0x0201
#define CMD_GetRelayOutputsOutput                                   0x8201
#define CMD_SetRelayOutputStateInput                                0x0280
#define CMD_SetRelayOutputStateOutput                               0x8280
#define CMD_SetRelayOutputSettingsInput                             0x0281
#define CMD_SetRelayOutputSettingsOutput                            0x8281
//=============================================================================
//						Imaging Service Definition
//=============================================================================
#define CMD_GetImagingSettingsInput                                 0x0300
#define CMD_GetImagingSettingsOutput                                0x8300
#define CMD_IMG_GetStatusInput                                      0x0301
#define CMD_IMG_GetStatusOutput                                     0x8301
#define CMD_IMG_GetOptionsInput                                     0x0302
#define CMD_IMG_GetOptionsOutput                                    0x8302
#define CMD_GetUserDefinedSettingsInput                             0x037F
#define CMD_GetUserDefinedSettingsOutput                            0x837F
#define CMD_SetImagingSettingsInput                                 0x0380
#define CMD_SetImagingSettingsOutput                                0x8380
#define CMD_IMG_MoveInput                                           0x0381
#define CMD_IMG_MoveOutput                                          0x8381
#define CMD_IMG_StopInput                                           0x0382
#define CMD_IMG_StopOutput                                          0x8382
#define CMD_SetUserDefinedSettingsInput                             0x03FF
#define CMD_SetUserDefinedSettingsOutput                            0x83FF
//=============================================================================
//						Media Service Definition
//=============================================================================
#define CMD_GetProfilesInput                                         0x0400
#define CMD_GetProfilesOutput                                        0x8400
#define CMD_GetVideoSourcesInput                                     0x0401
#define CMD_GetVideoSourcesOutput                                    0x8401
#define CMD_GetVideoSourceConfigurationsInput                        0x0402
#define CMD_GetVideoSourceConfigurationsOutput                       0x8402
#define CMD_GetGuaranteedNumberOfVideoEncoderInstancesInput          0x0403
#define CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput         0x8403
#define CMD_GetVideoEncoderConfigurationsInput                       0x0404
#define CMD_GetVideoEncoderConfigurationsOutput                      0x8404
#define CMD_GetAudioSourcesInput                                     0x0405
#define CMD_GetAudioSourcesOutput                                    0x8405
#define CMD_GetAudioSourceConfigurationsInput                        0x0406
#define CMD_GetAudioSourceConfigurationsOutput                       0x8406
#define CMD_GetAudioEncoderConfigurationsInput                       0x0407
#define CMD_GetAudioEncoderConfigurationsOutput                      0x8407
#define CMD_GetVideoSourceConfigurationOptionsInput                  0x0408
#define CMD_GetVideoSourceConfigurationOptionsOutput                 0x8408
#define CMD_GetVideoEncoderConfigurationOptionsInput                 0x0409
#define CMD_GetVideoEncoderConfigurationOptionsOutput                0x8409
#define CMD_GetAudioSourceConfigurationOptionsInput                  0x040A
#define CMD_GetAudioSourceConfigurationOptionsOutput                 0x840A
#define CMD_GetAudioEncoderConfigurationOptionsInput                 0x040B
#define CMD_GetAudioEncoderConfigurationOptionsOutput                0x840B
#define CMD_GetVideoOSDConfigurationInput                            0x0440
#define CMD_GetVideoOSDConfigurationOutput                           0x8440
#define CMD_GetVideoPrivateAreaInput                                 0x0441
#define CMD_GetVideoPrivateAreaOutput                                0x8441
#define CMD_SetSynchronizationPointInput                             0x0480
#define CMD_SetSynchronizationPointOutput                            0x8480
#define CMD_SetVideoSourceConfigurationInput                         0x0482
#define CMD_SetVideoSourceConfigurationOutput                        0x8482
#define CMD_SetVideoEncoderConfigurationInput                        0x0484
#define CMD_SetVideoEncoderConfigurationOutput                       0x8484
#define CMD_SetAudioSourceConfigurationInput                         0x0486
#define CMD_SetAudioSourceConfigurationOutput                        0x8486
#define CMD_SetAudioEncoderConfigurationInput                        0x0487
#define CMD_SetAudioEncoderConfigurationOutput                       0x8487
#define CMD_SetVideoOSDConfigurationInput                            0x04C0
#define CMD_SetVideoOSDConfigurationOutput                           0x84C0
#define CMD_SetVideoPrivateAreaInput                                 0x04C1
#define CMD_SetVideoPrivateAreaOutput                                0x84C1
#define CMD_SetVideoSourceControlInput                               0x04C2
#define CMD_SetVideoSourceControlOutput                              0x84C2
//=============================================================================
//						PTZ Service Definition
//=============================================================================
#define CMD_GetConfigurationsInput                                   0x0500
#define CMD_GetConfigurationsOutput                                  0x8500
#define CMD_PTZ_GetStatusInput                                       0x0501
#define CMD_PTZ_GetStatusOutput                                      0x8501
#define CMD_GetPresetsInput                                          0x0502
#define CMD_GetPresetsOutput                                         0x8502
#define CMD_GotoPresetInput                                          0x0580
#define CMD_GotoPresetOutput                                         0x8580
#define CMD_RemovePresetInput                                        0x0581
#define CMD_RemovePresetOutput                                       0x8581
#define CMD_SetPresetInput                                           0x0582
#define CMD_SetPresetOutput                                          0x8582
#define CMD_AbsoluteMoveInput                                        0x0583
#define CMD_AbsoluteMoveOutput                                       0x8583
#define CMD_RelativeMoveInput                                        0x0584
#define CMD_RelativeMoveOutput                                       0x8584
#define CMD_ContinuousMoveInput                                      0x0585
#define CMD_ContinuousMoveOutput                                     0x8585
#define CMD_SetHomePositionInput                                     0x0586
#define CMD_SetHomePositionOutput                                    0x8586
#define CMD_GotoHomePositionInput                                    0x0587
#define CMD_GotoHomePositionOutput                                   0x8587
#define CMD_PTZ_StopInput                                            0x0588
#define CMD_PTZ_StopOutput                                           0x8588
//=============================================================================
//						Video Analytics Service Definition
//=============================================================================
#define CMD_GetSupportedRulesInput                                   0x0600
#define CMD_GetSupportedRulesOutput                                  0x8600
#define CMD_GetRulesInput                                            0x0601
#define CMD_GetRulesOutput                                           0x8601
#define CMD_CreateRuleInput                                          0x0680
#define CMD_CreateRuleOutput                                         0x8680
#define CMD_ModifyRuleInput                                          0x0681
#define CMD_ModifyRuleOutput                                         0x8681
#define CMD_DeleteRuleInput                                          0x0682
#define CMD_DeleteRuleOutput                                         0x8682
//=============================================================================
//						Video Analytics Service Definition
//=============================================================================
#define CMD_MetadataStreamOutput                                     0xF000
//=============================================================================
//						Metadata Stream Service Definition
//=============================================================================
#define CMD_GetMetadataSettingsInput                                 0xE001
#define CMD_GetMetadataSettingsOutput                                0xF001
#define CMD_SetMetadataSettingsInput                                 0xE081
#define CMD_SetMetadataSettingsOutput                                0xF081

#endif

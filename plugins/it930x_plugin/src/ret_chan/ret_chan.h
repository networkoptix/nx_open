#ifndef RETURN_CHANNEL_H
#define RETURN_CHANNEL_H

#include "rc_cmd_ids.h"
#include "rc_type.h"

//------------------General-----------------

struct RCString
{
    RCString()
    :   stringLength(0),
        stringData(nullptr)
    {}

    RCString(const Byte * buf, unsigned length)
    :   stringLength(0),
        stringData(nullptr)
    {
        set(buf, length);
    }

    unsigned set(const Byte * buf, unsigned length);
    unsigned clear();
    unsigned copy(const RCString * srcStr);

    unsigned length() const { return stringLength; }
    Byte * data() const
    {
        if (stringData)
            return stringData;
        return (Byte *)"";
    }

    static unsigned readLength(const Byte *);

private:
    unsigned stringLength;
    Byte * stringData;

#if 0
    ~RCString()
    {
        clear();
    }

private:
    RCString(const RCString&);
    RCString& operator = (const RCString&);
#endif
};

//

typedef struct {
	RCString userName;
	RCString password;
} Security;

//------------------General-----------------

//-----------ccHDtv Service----------------
typedef struct {
	Byte IDType;
	Word deviceAddressID;
}NewTxDevice;

typedef struct {
	Byte processor;
	unsigned registerAddress;
	Byte* registerValues;
	Byte valueListSize;
}HwRegisterInfo;

typedef struct {
	Word PTS_PCR_delayTime;
	Word timeInterval;
	Byte skipNumber;
	Byte overFlowNumber;
	Word overFlowSize;
	Byte extensionFlag;
	Word Rx_LatencyRecoverTimeInterval;
	Word SIPSITableDuration;
	char frameRateAdjustment;
	Byte repeatPacketMode;
	Byte repeatPacketNum;
	Byte repeatPacketTimeInterval;
	Byte TS_TableDisable;
}AdvanceOptions;

typedef struct {
	Byte highCodeRate;
	Byte lowCodeRate;
	Byte transmissionMode;
	Byte constellation;
	Byte interval;
	Word cellID;
}TPSInfo;

typedef struct {
	RCString	manufactureName;
	RCString	modelName;
	RCString	firmwareVersion;
	RCString	serialNumber;
	RCString	hardwareId;
}ManufactureInfo;

typedef struct {
	Byte bandwidthOptions;
	unsigned frequencyMin;
	unsigned frequencyMax;
	Byte constellationOptions;
	Byte FFTOptions;
	Byte codeRateOptions;
	Byte guardInterval;
	Byte attenuationMin;
	Byte attenuationMax;
	Byte extensionFlag;
	char attenuationMin_signed;
	char attenuationMax_signed;
	Word TPSCellIDMin;
	Word TPSCellIDMax;
	Byte channelNumMin;
	Byte channelNumMax;
	Byte bandwidthStrapping;
	Byte TVStandard;
	Byte segmentationMode;
	Byte oneSeg_Constellation;
	Byte oneSeg_CodeRate;
} TransmissionParameterCapabilities;

typedef struct {
    Byte	bandwidth;
    unsigned frequency;         /** Channel frequency in KHz.                                */
    Byte	constellation;      /** Constellation scheme (FFT mode) in use                   */
    Byte	FFT;				/** Number of carriers used for OFDM signal                  */
    Byte	codeRate;           /** FEC coding ratio of high-priority stream                 */
    Byte	interval;           /** Fraction of symbol length used as guard (Guard Interval) */
    Byte	attenuation;
    Byte	extensionFlag;
    char	attenuation_signed;
    Word	TPSCellID;
    Byte	channelNum;
    Byte	bandwidthStrapping;
    Byte	TVStandard;
    Byte	segmentationMode;
    Byte	oneSeg_Constellation;
    Byte	oneSeg_CodeRate;
} TransmissionParameter;

typedef struct {
	Word	ONID;
	Word	NID;
	Word	TSID;
	Byte	networkName[32];
	Byte extensionFlag;
	unsigned privateDataSpecifier;
	Byte NITVersion;
	Byte countryID;
	Byte languageID;
}PSITable;

typedef struct{
	Word	latitude;
	Word	longitude;
	Word	extentLatitude;
	Word	extentLongitude;
}NITLoacation;

typedef struct{
	Word	serviceID;
	Byte	enable;
	Word	LCN;
	RCString	serviceName;
	RCString	provider;
	Byte extensionFlag;
	Byte IDAssignationMode;
	Byte ISDBT_RegionID;
	Byte ISDBT_BroadcasterRegionID;
	Byte ISDBT_RemoteControlKeyID;
	Word ISDBT_ServiceIDDataType_1;
	Word ISDBT_ServiceIDDataType_2;
	Word ISDBT_ServiceIDPartialReception;
}ServiceConfig;

typedef struct{
	Byte accessOption;
	Byte tableType;
	RCString tableData;
}CalibrationTable;

typedef struct{
	Byte enable;
	unsigned startDate;
	unsigned startTime;
	unsigned duration;
	RCString eventName;
	RCString eventText;
}EITInfoParam;

typedef struct{
	RCString videoEncConfigToken;
	Byte listSize;
	EITInfoParam* eitInfoParam;
}EITInfo;

typedef struct{
	Byte listSize;
	EITInfo* infoList;
	Byte returnIndex;
}EITInfoList;
//-----------ccHDtv Service----------------

//-----------Device Management Service----------------

typedef struct {
	unsigned supportedFeatures;
} DeviceCapability;

typedef struct{
	RCString hostName;
}HostInfo;

typedef struct{
	Byte countryCode[3];
	Byte countryRegionID;
	Byte daylightSavings;
	Byte timeZone;
	Byte UTCHour;
	Byte UTCMinute;
	Byte UTCSecond;
	Word UTCYear;
	Byte UTCMonth;
	Byte UTCDay;
	Byte extensionFlag;
	Word UTCMillisecond;
	Byte timeAdjustmentMode;
	unsigned timeAdjustmentCriterionMax;
	unsigned timeAdjustmentCriterionMin;
	Word timeSyncDuration;
}SystemTime;

typedef struct{
	Byte logType;
	RCString logData;
}SystemLog;

typedef struct{
	Byte dateEnable;
	Byte datePosition;
	Byte dateFormat;
	Byte timeEnable;
	Byte timePosition;
	Byte timeFormat;
	Byte logoEnable;
	Byte logoPosition;
    Byte logoOption;
	Byte detailInfoEnable;
	Byte detailInfoPosition;
    Byte detailInfoOption;
	Byte textEnable;
	Byte textPosition;
	RCString text;
}OSDInfo;

typedef struct{
	Byte factoryDefault;
}SystemDefault;

typedef struct{
	Byte rebootType;
	RCString responseMessage;
}SystemReboot;

typedef struct{
	Byte firmwareType;
	RCString data;
	RCString message;
}SystemFirmware;
//-----------Device Management Service----------------

//-----------Device IO Service----------
typedef struct{
	Byte listSize;
	RCString* tokenList;
}DigitalInputsInfo;

typedef struct{
	RCString token;
	Byte mode;
	unsigned delayTime;
	Byte idleState;
}RelayOutputsParam;

typedef struct{
	Byte listSize;
	RelayOutputsParam* relayOutputsParam;
}RelayOutputs;

typedef struct{
	RCString token;
	Byte logicalState;
}RelayOutputState;
//-----------Device IO Service----------

//-----------Imaging Service---------
typedef struct{
	RCString videoSourceToken;
	Byte backlightCompensationMode;
	float backlightCompensationLevel;
	float brightness;
	float colorSaturation;
	float contrast;
	Byte exposureMode;
	Byte exposurePriority;
	Word exposureWindowbottom;
	Word exposureWindowtop;
	Word exposureWindowright;
	Word exposureWindowleft;
	unsigned minExposureTime;
	unsigned maxExposureTime;
	float exposureMinGain;
	float exposureMaxGain;
	float exposureMinIris;
	float exposureMaxIris;
	unsigned exposureTime;
	float exposureGain;
	float exposureIris;
	Byte autoFocusMode;
	float focusDefaultSpeed;
	float focusNearLimit;
	float focusFarLimit;
	Byte irCutFilterMode;
	float sharpness;
	Byte wideDynamicRangeMode;
	float wideDynamicRangeLevel;
	Byte whiteBalanceMode;
	float whiteBalanceCrGain;
	float whiteBalanceCbGain;
	Byte analogTVOutputStandard;
	float imageStabilizationLevel;
	Byte flickerControl;
	Byte extensionFlag;
	Byte imageStabilizationMode;
	Byte deNoiseMode;
	float deNoiseStrength;
	Byte backLightControlMode;
	float backLightControlStrength;
	Byte forcePersistence;
}ImageConfig;

typedef struct{
	Byte listSize;
	ImageConfig* configList;
	Byte returnIndex;
}ImageConfigList;

typedef struct{
	RCString videoSourceToken;
	unsigned position;
	Byte moveStatus;
	RCString error;
}FocusStatusInfo;

typedef struct{
	Byte listSize;
	FocusStatusInfo* infoList;
	Byte returnIndex;
}FocusStatusInfoList;

typedef struct{
	RCString videoSourceToken;
	float absolutePosition;
	float absoluteSpeed;
	float relativeDistance;
	float relativeSpeed;
	float continuousSpeed;
}FocusMoveInfo;

typedef struct{
	Byte listSize;
	FocusMoveInfo* infoList;
}FocusMoveInfoList;

typedef struct{
	RCString videoSourceToken;
}FocusStopInfo;

typedef struct{
	Byte listSize;
	FocusStopInfo* infoList;
}FocusStopInfoList;

typedef struct{
	RCString videoSourceToken;
	Byte backlightCompensationMode;
	float backlightCompensationLevelMin;
	float backlightCompensationLevelMax;
	float brightnessMin;
	float brightnessMax;
	float colorSaturationMin;
	float colorSaturationMax;
	float contrastMin;
	float contrastMax;
	Byte exposureMode;
	Byte exposurePriority;
	unsigned minExposureTimeMin;
	unsigned minExposureTimeMax;
	unsigned maxExposureTimeMin;
	unsigned maxExposureTimeMax;
	float exposureMinGainMin;
	float exposureMinGainMax;
	float exposureMaxGainMin;
	float exposureMaxGainMax;
	float exposureMinIrisMin;
	float exposureMinIrisMax;
	float exposureMaxIrisMin;
	float exposureMaxIrisMax;
	unsigned exposureTimeMin;
	unsigned exposureTimeMax;
	float exposureGainMin;
	float exposureGainMax;
	float exposureIrisMin;
	float exposureIrisMax;
	Byte autoFocusMode;
	float focusDefaultSpeedMin;
	float focusDefaultSpeedMax;
	float focusNearLimitMin;
	float focusNearLimitMax;
	float focusFarLimitMin;
	float focusFarLimitMax;
	Byte irCutFilterMode;
	float sharpnessMin;
	float sharpnessMax;
	Byte wideDynamicRangeMode;
	float wideDynamicRangeLevelMin;
	float wideDynamicRangeLevelMax;
	Byte whiteBalanceMode;
	float whiteBalanceCrGainMin;
	float whiteBalanceCrGainMax;
	float whiteBalanceCbGainMin;
	float whiteBalanceCbGainMax;
	Byte imageStabilizationMode;
	float imageStabilizationLevelMin;
	float imageStabilizationLevelMax;
	Byte flickerControl;
	Byte analogTVOutputStandard;
	Byte deNoiseMode;
	float deNoiseStrengthMin;
	float deNoiseStrengthMax;
	Byte backLightControlMode;
	float backLightControlStrengthMin;
	float backLightControlStrengthMax;
}ImageConfigOption;

typedef struct{
	Byte listSize;
	ImageConfigOption* configList;
	Byte returnIndex;
}ImageConfigOptionList;

typedef struct{
	RCString videoSourceToken;
	RCString uerDefinedData;
}UserDefinedSettings;

typedef struct{
	Byte listSize;
	UserDefinedSettings* settingList;
	Byte returnIndex;
}UserDefinedSettingsList;
//-----------Imaging Service---------

//----------- Media Service----------
typedef struct{
	RCString token;
	RCString name;
	Byte useCount;
	Byte encoding;
	Word bitrate;
	Word sampleRate;
	Byte forcePersistence;
}AudioEncConfigParam;

typedef struct{
	Byte configListSize;
	AudioEncConfigParam* configList;
}AudioEncConfig;

typedef struct{
	RCString videoSrcConfigToken;
	RCString profileToken;
	Word boundsRange_X_Min;
	Word boundsRange_X_Max;
	Word boundsRange_Y_Min;
	Word boundsRange_Y_Max;
	Word boundsRange_Width_Min;
	Word boundsRange_Width_Max;
	Word boundsRange_Height_Min;
	Word boundsRange_Heigh_Max;
	Byte videoSrcTokensAvailableListSize;
	RCString* videoSrcTokensAvailableList;
	Byte rotateModeOptions;
	Word rotateDegreeMinOption;
	Byte mirrorModeOptions;
	Byte maxFrameRateMin;
	Byte maxFrameRateMax;
}VideoSrcConfigOptions;

typedef struct{
	Byte listSize;
	VideoSrcConfigOptions* configList;
	Byte returnIndex;
}VideoSrcConfigOptionsList;

typedef struct{
	RCString audioSrcConfigToken;
	RCString profileToken;
	Byte audioSrcTokensAvailableListSize;
	RCString* audioSrcTokensAvailableList;
}AudioSrcConfigOptions;

typedef struct{
	Byte listSize;
	AudioSrcConfigOptions* configList;
	Byte returnIndex;
}AudioSrcConfigOptionsList;

typedef struct{
	Byte encodingOption;
	Word bitrateRangeMin;
	Word bitrateRangeMax;
	Byte sampleRateAvailableListSize;
	Word* sampleRateAvailableList;
}AudioEncConfigOptionsParam;

typedef struct{
	RCString audioEncConfigToken;
	RCString profileToken;
	Byte configListSize;
	AudioEncConfigOptionsParam* audioEncConfigOptionsParam;
}AudioEncConfigOptions;

typedef struct{
	Byte listSize;
	AudioEncConfigOptions* configList;
	Byte returnIndex;
}AudioEncConfigOptionsList;

typedef struct{
	Word width;
	Word height;
}ResolutionsAvailableList;

typedef struct{
	RCString videoEncConfigToken;
	RCString profileToken;
	Byte encodingOption;
	Byte resolutionsAvailableListSize;
	ResolutionsAvailableList* resolutionsAvailableList;
	Byte qualityMin;
	Byte qualityMax;
	Byte frameRateMin;
	Byte frameRateMax;
	Byte encodingIntervalMin;
	Byte encodingIntervalMax;
	Word bitrateRangeMin;
	Word bitrateRangeMax;
	Byte rateControlTypeOptions;
	Byte govLengthRangeMin;
	Byte govLengthRangeMax;
	Byte profileOptions;
	Word targetBitrateRangeMin;
	Word targetBitrateRangeMax;
	Byte aspectRatioAvailableListSize;
	Word* aspectRatioList;
}VideoEncConfigOptions;

typedef struct{
	Byte listSize;
	VideoEncConfigOptions* configList;
	Byte returnIndex;
}VideoEncConfigOptionsList;

typedef struct{
	RCString name;
	Byte useCount;
	RCString token;
	RCString outputToken;
	Byte sendPrimacy;
	unsigned outputLevel;
}AudioOutputConfigParam;

typedef struct{
	RCString name;
	RCString token;
	RCString sourceToken;
	Byte useCount;
	Byte forcePersistence;
}AudioSrcConfigParam;

typedef struct{
	Byte configListSize;
	AudioSrcConfigParam* audioSrcConfigList;
}AudioSrcConfig;

typedef struct{
	RCString audioSourcesToken;
	Byte channels;
}AudioSourcesParam;

typedef struct{
	Byte audioSourcesListSize;
	AudioSourcesParam* audioSourcesList;
}AudioSources;

typedef struct{
	RCString configurationToken;
	Byte TotallNum;
	Byte JPEGNum;
	Byte H264Num;
	Byte MPEG4Num;
	Byte MPEG2Num;
}GuaranteedEncs;

typedef struct{
	Byte listSize;
	GuaranteedEncs* configList;
	Byte returnIndex;
}GuaranteedEncsList;

typedef struct{
	RCString simpleItemName;
	RCString simpleItemValue;
	RCString elementItemName;
	Byte extensionItemListSize;
	RCString* extensionItemList;
	RCString name;
	RCString type;
}MediaProfilesParamsConfig;

typedef struct{
	RCString name;
	RCString token;
	RCString videoSrcName;
	RCString videoSrcToken;
	RCString videoSrcSourceToken;
	Byte videoSrcUseCount;
	Word videoSrcBounds_x;
	Word videoSrcBounds_y;
	Word videoSrcBoundsWidth;
	Word videoSrcBoundsHeight;
	Byte videoSrcRotateMode;
	Word videoSrcRotateDegree;
	Byte videoSrcMirrorMode;
	RCString audioSrcName;
	RCString audioSrcToken;
	RCString audioSrcSourceToken;
	Byte audioSrcUseCount;
	RCString videoEncName;
	RCString videoEncToken;
	Byte videoEncUseCount;
	Byte videoEncEncodingMode;
	Word videoEncResolutionWidth;
	Word videoEncResolutionHeight;
	Byte videoEncQuality;
	Byte videoEncRateControlFrameRateLimit;
	Byte videoEncRateControlEncodingInterval;
	Word videoEncRateControlBitrateLimit;
	Byte videoEncRateControlType;
	Byte videoEncGovLength;
	Byte videoEncProfile;
	RCString audioEncName;
	RCString audioEncToken;
	Byte audioEncUseCount;
	Byte audioEncEncoding;
	Word audioEncBitrate;
	Word audioEncSampleRate;
	RCString audioOutputName;
	RCString audioOutputToken;
	RCString audioOutputOutputToken;
	Byte audioOutputUseCount;
	Byte audioOutputSendPrimacy;
	Byte audioOutputOutputLevel;
	RCString audioDecName;
	RCString audioDecToken;
	Byte audioDecUseCount;
	Word videoEncConfigRateControlTargetBitrateLimit;
}MediaProfilesParam;

typedef struct{
	Byte profilesListSize;
	MediaProfilesParam* mediaProfilesParam;
	Byte extensionFlag;
}MediaProfiles;

typedef struct{
	unsigned Width;
	unsigned Height;
}ResolutionsAvailableParam;

typedef struct{
	RCString name;
	RCString token;
	Byte useCount;
	Byte encoding;
	Word width;
	Word height;
	Byte quality;
	Byte frameRateLimit;
	Byte encodingInterval;
	Word bitrateLimit;
	Byte rateControlType;
	Byte govLength;
	Byte profile;
	Byte extensionFlag;
	Word targetBitrateLimit;
	Word aspectRatio;
	Byte forcePersistence;
}VideoEncConfigParam;

typedef struct{
	Byte configListSize;
	VideoEncConfigParam* configList;
	Byte extensionFlag;
}VideoEncConfig;

typedef struct{
	RCString name;
	RCString token;
	RCString srcToken;
	Byte useCount;
	Word bounds_x;
	Word bounds_y;
	Word boundsWidth;
	Word boundsHeight;
	Byte rotateMode;
	Word rotateDegree;
	Byte mirrorMode;
	Byte extensionFlag;
	Byte maxFrameRate;
	Byte forcePersistence;
}VideoSrcConfigParam;

typedef struct{
	Byte configListSize;
	VideoSrcConfigParam* configList;
	Byte extensionFlag;
}VideoSrcConfig;

typedef struct{
	RCString token;
	Byte frameRate;
	Word resolutionWidth;
	Word resolutionHeight;
}VideoSrcParam;

typedef struct{
	Byte videoSrcListSize;
	VideoSrcParam* srcList;
}VideoSrc;

typedef struct{
	RCString profileToken;
}SyncPoint;

typedef struct{
	Byte listSize;
	SyncPoint* configList;
}SyncPointList;

typedef struct{
	RCString videoSrcToken;
	Byte dateEnable;
	Byte datePosition;
	Byte dateFormat;
	Byte timeEnable;
	Byte timePosition;
	Byte timeFormat;
	Byte logoEnable;
	Byte logoPosition;
    Byte logoOption;
	Byte detailInfoEnable;
	Byte detailInfoPosition;
	Byte detailInfoOption;
	Byte textEnable;
	Byte textPosition;
	RCString text;
}VideoOSDConfig;

typedef struct{
	Byte listSize;
	VideoOSDConfig* configList;
	Byte returnIndex;
}VideoOSDConfigList;

typedef struct{
	Word polygon_x;
	Word polygon_y;
}PrivateAreaPolygon;

typedef struct{
	RCString videoSrcToken;
	Byte polygonListSize;
	PrivateAreaPolygon* privateAreaPolygon;
	Byte privateAreaEnable;
}VideoPrivateArea;

typedef struct{
	Byte listSize;
	VideoPrivateArea* areaList;
	Byte returnIndex;
}VideoPrivateAreaList;

typedef struct{
	RCString videoSrcToken;
	Byte controlCommand;
}VideoSrcControl;

typedef struct{
	Byte listSize;
	VideoSrcControl* controlList;
}VideoSrcControlList;
//----------- Media Service----------

//----------- PTZ  Service----------
typedef struct{
	RCString name;
	Byte useCount;
	RCString token;
	short defaultPanSpeed;
	short defaultTiltSpeed;
	short defaultZoomSpeed;
	unsigned defaultTimeout;
	Word panLimitMin;
	Word panLimitMax;
	Word tiltLimitMin;
	Word tiltLimitMax;
	Word zoomLimitMin;
	Word zoomLimitMax;
	Byte eFlipMode;
	Byte reverseMode;
	RCString videoSrcToken;
}PTZConfigParam;

typedef struct{
	Byte PTZConfigListSize;
	PTZConfigParam* PTZConfigList;
	Byte extensionFlag;
}PTZConfig;

typedef struct{
	RCString token;
	short panPosition;
	short tiltPosition;
	short zoomPosition;
	Byte panTiltMoveStatus ;
	Byte zoomMoveStatus;
	RCString error;
	Byte UTCHour;
	Byte UTCMinute;
	Byte UTCSecond;
	Word UTCYear;
	Byte UTCMonth;
	Byte UTCDay;
}PTZStatus;

typedef struct{
	Byte listSize;
	PTZStatus* configList;
	Byte returnIndex;
}PTZStatusList;

typedef struct{
	RCString presetName;
	RCString presetToken;
	short panPosition;
	short tiltPosition;
	short zoomPosition;
	short panSpeed;
	short tiltSpeed;
	short zoomSpeed;
}PTZPresetsConfig;

typedef struct{
	RCString token;
	Byte configListSize;
	PTZPresetsConfig* configList;
}PTZPresets;

typedef struct{
	Byte presetListSize;
	PTZPresets* presetList;
	Byte returnIndex;
}PTZPresetsList;

typedef struct{
	RCString token;
	RCString presetToken;
	short panSpeed;
	short tiltSpeed;
	short zoomSpeed;
}PTZGotoParam;

typedef struct{
	RCString token;
	RCString presetName;
	RCString presetToken;
	RCString presetToken_set;
}PTZPresetsSet;

typedef struct{
	RCString token;
	RCString presetToken;
}PTZRemoveParam;

typedef struct{
	RCString token;
	short panPosition;
	short tiltPosition;
	short zoomPosition;
	short panSpeed;
	short tiltSpeed;
	short zoomSpeed;
}PTZAbsoluteMove;

typedef struct{
	RCString token;
	short panTranslation;
	short tiltTranslation;
	short zoomTranslation;
	short panSpeed;
	short tiltSpeed;
	short zoomSpeed;
}PTZRelativeMove;

typedef struct{
	RCString token;
	short panVelocity;
	short tiltVelocity;
	short zoomVelocity;
	unsigned timeout;
}PTZContinuousMove;

typedef struct{
	RCString token;
	short panPosition;
	short tiltPosition;
	short zoomPosition;
	short panSpeed;
	short tiltSpeed;
	short zoomSpeed;
}PTZHomePosition;

typedef struct{
	RCString token;
	Byte panTiltZoom;
}PTZStop;
//----------- PTZ  Service----------

//------Video Analytics Service------
typedef struct{
	Byte supportListSize;
	Byte* ruleType;
}SupportedRules;

typedef struct{
	Word polygon_x;
	Word polygon_y;
}DetectPolygon;

typedef struct{
	Byte motionExpression;
	Byte polygonListSize;
	DetectPolygon* detectPolygon;
	Byte metadataStreamSwitch;
}Rule_MotionDetector;

typedef struct{
	Word direction;
	Byte polygonListSize;
	DetectPolygon* detectPolygon;
	Byte metadataStreamSwitch;
}Rule_LineDetector;

typedef struct{
	Byte polygonListSize;
	DetectPolygon* detectPolygon;
	Byte metadataStreamSwitch;
}Rule_FieldDetector;

typedef struct{
	unsigned reportTimeInterval;
	unsigned resetTimeInterval;
	Word direction;
	Byte polygonListSize;
	DetectPolygon* detectPolygon;
	Byte metadataStreamSwitch;
}Rule_Counting ;

typedef struct{
	Word minCount;
	unsigned alarmOnDelay;
	unsigned alarmOffDelay;
	Word activeCellsSize;
	RCString activeCells;
	Byte sensitivity;
	Word layoutBounds_x;
	Word layoutBounds_y;
	Word layoutBounds_width;
	Word layoutBounds_height;
	Byte layoutColumns;
	Byte layoutRows;
	Byte metadataStreamSwitch;
}Rule_CellMotion ;

typedef struct{
	RCString ruleToken;
}DeleteRule;

typedef struct RuleInfo{
	RCString ruleToken;
	RCString ruleName;
	Byte type;
	void* rule;
	Byte extensionFlag;
	RCString videoSrcToken;
	Byte threshold;
	Byte motionSensitivity;
}TotalRule;

typedef struct{
	Byte ruleListSize;
	TotalRule* totalRuleList;
	Byte maxListSize;
	Byte extensionFlag;
}RuleList;
//------Video Analytics Service------

//-----Metadata Stream Service----
typedef struct{
	Word deviceVendorID;
	Word deviceModelID;
	Word HWVersionCode;
	unsigned SWVersionCode;
	RCString textInformation;
	Byte extensionFlag;
	unsigned baudRate;
}Metadata_Device;

typedef struct{
	Word videoPID;
	Byte videoEncodingType;
	Word videoResolutionWidth;
	Word videoResolutionHeight;
	Byte videoFrameRate;
	Word videoBitrate;
	Word audioPID;
	Byte audioEncodingType;
	Word audioBitrate;
	Word audioSampleRate;
	Word PCRPID;
	RCString videoSrcToken;
}Metadata_StreamParam;

typedef struct{
	Byte streamInfoListSize;
	Metadata_StreamParam*	streamInfoList;
	Byte extensionFlag;
}Metadata_Stream;

typedef struct{
	RCString ruleToken;
	unsigned objectID;
	Byte UTCHour;
	Byte UTCMinute;
	Byte UTCSecond;
	Word UTCYear;
	Byte UTCMonth;
	Byte UTCDay;
	Byte IsInside;
	unsigned count;
	Byte IsMotion;
	Byte extensionFlag;
	RCString videoSrcToken;
}Metadata_Event;

typedef struct{
	Word version;
	Byte type;
	Metadata_Device metadata_Device;
	Metadata_Stream metadata_Stream;
	Metadata_Event metadata_Event;
}MetadataStreamInfo;

typedef struct{
	Byte deviceInfoEnable;
	Word deviceInfoPeriod;
	Byte streamInfoEnable;
	Word streamInfoPeriod;
	Byte motionDetectorEnable;
	Word motionDetectorPeriod;
}MetadataSettings;

//

struct TxRC
{
    TxRC();
    ~TxRC();

    //--------------General-------------------
    Security								security;
    //--------------General-------------------

    //-----------ccHDtv Service--------------
    NewTxDevice								newTxDevice;
    HwRegisterInfo							hwRegisterInfo;
    AdvanceOptions							advanceOptions;
    TPSInfo									tpsInfo;
    TransmissionParameterCapabilities		transmissionParameterCapabilities;
    TransmissionParameter					transmissionParameter;
    PSITable								psiTable;
    NITLoacation							nitLoacation;
    ServiceConfig							serviceConfig;
    CalibrationTable						calibrationTable;
    EITInfo									eitInfo;
    //-----------ccHDtv Service--------------

    //-------Device Management Service-----
    DeviceCapability						deviceCapability;
    ManufactureInfo							manufactureInfo;
    HostInfo								hostInfo;
    SystemTime								systemTime;
    SystemLog								systemLog;
    OSDInfo									osdInfo;
    SystemDefault							systemDefault;
    SystemReboot							systemReboot;
    SystemFirmware							systemFirmware;
    //-------Device Management Service-----

    //----------Device_IO Service-----------
    DigitalInputsInfo						digitalInputsInfo;
    RelayOutputs							relayOutputs;
    RelayOutputsParam						relayOutputsSetParam;
    RelayOutputState						relayOutputState;
    //----------Device_IO Service-----------

    //------------------Imaging Service---------------------
    ImageConfig								imageConfig;
    FocusStatusInfo							focusStatusInfo;
    FocusMoveInfo							focusMoveInfo;
    FocusStopInfo							focusStopInfo;
    ImageConfigOption						imageConfigOption;
    UserDefinedSettings						userDefinedSettings;
    //------------------Imaging Service---------------------

    //------------------Media Service------------------
    AudioEncConfig							audioEncConfig;
    AudioSrcConfig							audioSrcConfig;
    AudioSources							audioSources;
    GuaranteedEncs							guaranteedEncs;
    MediaProfiles							mediaProfiles;
    VideoEncConfig							videoEncConfig;
    VideoSrcConfig							videoSrcConfig;
    VideoSrc								videoSrc;
    VideoSrcConfigOptions					videoSrcConfigOptions;
    VideoEncConfigOptions					videoEncConfigOptions;
    AudioSrcConfigOptions					audioSrcConfigOptions;
    AudioEncConfigOptions					audioEncConfigOptions;
    AudioEncConfigParam						audioEncConfigSetParam;
    AudioSrcConfigParam						audioSrcConfigSetParam;
    VideoEncConfigParam						videoEncConfigSetParam;
    VideoSrcConfigParam						videoSrcConfigSetParam;
    SyncPoint								syncPoint;
    VideoOSDConfig							videoOSDConfig;
    VideoPrivateArea						videoPrivateArea;
    VideoSrcControl							videoSrcControl;
    //------------------Media Service------------------

    //-------------------PTZ Service-------------------
    PTZConfig								ptzConfig;
    PTZStatus								ptzStatus;
    PTZPresets								ptzPresetsGet;
    PTZGotoParam							ptzGotoParam;
    PTZPresetsSet							ptzPresetsSet;
    PTZRemoveParam							ptzRemoveParam;
    PTZAbsoluteMove							ptzAbsoluteMove;
    PTZRelativeMove							ptzRelativeMove;
    PTZContinuousMove						ptzContinuousMove;
    PTZHomePosition							ptzHomePosition;
    PTZStop									ptzStop;
    //-------------------PTZ Service-------------------

    SupportedRules							supportedRules;
    TotalRule								totalRule;
    DeleteRule								deleteRule;
    RuleList								ruleList;

    //------------Metadata Stream Service------------
    MetadataStreamInfo						metadataStreamInfo;
    MetadataSettings						metadataSettings;
    //------------Metadata Stream Service------------
};

typedef TxRC RCHostInfo;

//

void Cmd_CheckByteIndexRead(IN const Byte * buf, IN unsigned length, OUT unsigned * var);

unsigned Cmd_DwordRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT unsigned * var, IN unsigned defaultValue);
unsigned Cmd_QwordRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT unsigned long long * var, IN unsigned long long defaultValue);
unsigned Cmd_StringRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT RCString * str);
unsigned Cmd_BytesRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT Byte* bufDst, IN unsigned dstLength);
unsigned Cmd_ByteRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT Byte * var, IN Byte defaultValue);
unsigned Cmd_WordRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT Word * var, IN Word defaultValue);
unsigned Cmd_FloatRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT float * var, IN float defaultValue);
unsigned Cmd_ShortRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT short * var, IN short defaultValue);
unsigned Cmd_CharRead(IN const Byte * buf, IN unsigned bufferLength, IN unsigned * index, OUT char* var, IN char defaultValue);
void Cmd_WordAssign(IN Byte * buf, IN Word var, IN unsigned * length);
void Cmd_DwordAssign(IN Byte * buf, IN unsigned var, IN unsigned * length);
void Cmd_QwordAssign(IN Byte * buf, IN unsigned long long var, IN unsigned * length);
void Cmd_StringAssign(IN Byte * buf, IN const RCString * str, IN unsigned * length);
void Cmd_FloatAssign(IN Byte * buf, IN float var, IN unsigned * length);
void Cmd_ShortAssign(IN Byte * buf, IN short var, IN unsigned * length);

#endif

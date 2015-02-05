#ifndef __TYPE_H__
#define __TYPE_H__


//#define DllExport __declspec (dllexport)
//#define DllImport __declspec (dllimport)
//#define DemodulatorStatus extern "C" DllExport Dword
#define IN
#define OUT
#define INOUT


/**
 * Temp defination of Gemini
 */
#define MEMORY_INDICATOR_DATA		0x00800000ul	// stream, packet data
#define MEMORY_INDICATOR_MCI			0x00000001ul
#define MEMORY_INDICATOR_SI			0x00000002ul
#define MEMORY_INDICATOR_FIGBYPASS	0x00000004ul
#define MEMORY_INDICATOR_PAGGING		0x00000008ul


/**
 * The type of handle.
 */
typedef void* Handle;


/**
 * The type defination of 8-bits unsigned type.
 */
typedef unsigned char Byte;


/**
 * The type defination of 16-bits unsigned type.
 */
typedef unsigned short Word;


/**
 * The type defination of 32-bits unsigned type.
 */
typedef unsigned long Dword;
typedef unsigned int u32;

/**
 * The type defination of 16-bits signed type.
 */
typedef short Short;


/**
 * The type defination of 32-bits signed type.
 */
typedef long Long;


/**
 * The type defination of Bool
 */
typedef enum {
	False = 0,
	True = 1
} Bool;


/**
 * The type defination of Segment
 */
typedef struct {
	Byte segmentType;
	Dword segmentLength;
} Segment;


/**
 * The type defination of Bandwidth.
 */
typedef enum {
    Bandwidth_6M = 0,
    Bandwidth_7M,
    Bandwidth_8M,
    Bandwidth_5M
} Bandwidth;


/**
 * The type defination of Mode.
 */
typedef enum {
    Mode_QPSK = 0,
    Mode_16QAM,
    Mode_64QAM
} Mode;


/**
 * The type defination of Fft.
 */
typedef enum {
    Fft_2K = 0,
    Fft_8K = 1,
    Fft_4K = 2
} Fft;


/**
 * The type defination of Interval.
 */
typedef enum {
    Interval_1_OVER_32 = 0,     // 1/32
    Interval_1_OVER_16,         // 1/16
    Interval_1_OVER_8,          // 1/8
    Interval_1_OVER_4           // 1/4
} Interval;


/**
 * The type defination of Priority.
 */
typedef enum {
    Priority_HIGH = 0,
    Priority_LOW
} Priority;             // High Priority or Low Priority


/**
 * The type defination of CodeRate.
 */
typedef enum {
    CodeRate_1_OVER_2 = 0,  // 1/2
    CodeRate_2_OVER_3,      // 2/3
    CodeRate_3_OVER_4,      // 3/4
    CodeRate_5_OVER_6,      // 5/6
    CodeRate_7_OVER_8,      // 7/8
    CodeRate_NONE         // none, NXT doesn't have this one
} CodeRate;               // Code Rate


/**
 * TPS Hierarchy and Alpha value.
 */
typedef enum {
    Hierarchy_NONE = 0,
    Hierarchy_ALPHA_1,
    Hierarchy_ALPHA_2,
    Hierarchy_ALPHA_4
} Hierarchy;


/**
 * The defination of SubchannelType.
 */
typedef enum {
	SubchannelType_AUDIO = 0,
	SubchannelType_VIDEO = 1,
	SubchannelType_PACKET = 3,
	SubchannelType_ENHANCEPACKET = 4
} SubchannelType;


/**
 * The defination of ProtectionLevel.
 */
typedef enum {
	ProtectionLevel_NONE = 0x00,
	ProtectionLevel_PL1 = 0x01,
	ProtectionLevel_PL2 = 0x02,
	ProtectionLevel_PL3 = 0x03,
	ProtectionLevel_PL4 = 0x04,
	ProtectionLevel_PL5 = 0x05,
	ProtectionLevel_PL1A = 0x1A,
	ProtectionLevel_PL2A = 0x2A,
	ProtectionLevel_PL3A = 0x3A,
	ProtectionLevel_PL4A = 0x4A,
	ProtectionLevel_PL1B = 0x1B,
	ProtectionLevel_PL2B = 0x2B,
	ProtectionLevel_PL3B = 0x3B,
	ProtectionLevel_PL4B = 0x4B
} ProtectionLevel;


/**
 * The value of this variable denote which demodulator is using.
 */
typedef struct {
    Dword frequency;
    Mode mode;
    Fft fft;
    Interval interval;
	Priority priority;
    CodeRate highCodeRate;
    CodeRate lowCodeRate;
    Hierarchy hierarchy;
    Bandwidth bandwidth;
} ChannelModulation;


/**
 * The defination of SubchannelStatus.
 */
typedef struct {
	Byte subchannelId;
	Word subchannelSize;
	Word bitRate;
	Byte transmissionMode;				// transmissionMode = 1, 2, 3, 4
	ProtectionLevel protectionLevel;
	SubchannelType subchannelType;
	Byte conditionalAccess;
	Byte tiiPrimary;
	Byte tiiCombination;
} SubchannelModulation;


/**
 * The type defination of IpVersion.
 */
typedef enum {
	IpVersion_IPV4 = 0,
	IpVersion_IPV6 = 1
} IpVersion;


/**
 * The type defination of Ip.
 */
typedef struct {
	IpVersion version;
	Priority priority;
	Bool cache;
	Byte address[16];
} Ip;


/**
 * The type defination of Platform.
 * Mostly used is in DVB-H standard
 */
typedef struct {
	Dword platformId;
	char iso639LanguageCode[3];
	Byte platformNameLength;
	char platformName[32];
	Word bandwidth;
	Dword frequency;
	Byte* information;
	Word informationLength;
	Bool hasInformation;
	IpVersion ipVersion;
} Platform;


/**
 * Temp data structure of Gemini.
 */
typedef struct
{
	Byte charSet;
	Word charFlag;
	Byte string[16];
} Label;


/**
 * Temp data structure of Gemini.
 */
typedef struct
{
	Word ensembleId;
	Label ensembleLabel;
	Byte totalServices;
} Ensemble;


/**
 * The type defination of Service.
 * Mostly used is in T-DMB standard
 */
typedef struct {
	Byte serviceType;		// Service Type(P/D): 0x00: Program, 0x80: Data
	Dword serviceId;
	Dword frequency;
	Label serviceLabel;
	Byte totalComponents;
} Service;


/**
 * The type defination of Service Component.
 */
typedef struct {
	Byte serviceType;			// Service Type(P/D): 0x00: Program, 0x80: Data
	Dword serviceId;			// Service ID
	Word componentId;			// Stream audio/data is subchid, packet mode is SCId
	Byte componentIdService;	// Component ID within Service
	Label componentLabel;
	Byte language;				// Language code
	Byte primary;				// Primary/Secondary
	Byte conditionalAccess;		// Conditional Access flag
	Byte componentType;			// Component Type (A/D)
	Byte transmissionId;		// Transmission Mechanism ID
} Component;


/**
 * The type defination of Target.
 */
typedef enum {
	SectionType_MPE = 0,
	SectionType_SIPSI,
	SectionType_TABLE
} SectionType;


/**
 * The type defination of Target.
 */
typedef enum {
	FrameRow_256 = 0,
	FrameRow_512,
	FrameRow_768,
	FrameRow_1024
} FrameRow;


/**
 * The type defination of Pid.
 */
typedef struct {
	Byte table;
	Byte duration;
	FrameRow frameRow;
	SectionType sectionType;
	Priority priority;
	IpVersion version;
	Bool cache;
	Word value;
} Pid;


/**
 * The type defination of ValueSet.
 */
typedef struct {
	Dword address;
	Byte value;
} ValueSet;


/**
 * The type defination of ValueSet.
 */
typedef struct {
	Dword address;
	Byte length;
	Byte* value;
} MultiValueSet;


/**
 * The type defination of Datetime.
 */
typedef struct {
	Dword mjd;
	Byte configuration;
	Byte hours;
	Byte minutes;
	Byte seconds;
	Word milliseconds;
} Datetime;


/**
 * The type defination of Interrupts.
 */
typedef Word Interrupts;


/**
 * The type defination of Interrupt.
 */
typedef enum {
	Interrupt_NONE		= 0x0000,
	Interrupt_SIPSI		= 0x0001,
	Interrupt_DVBH		= 0x0002,
	Interrupt_DVBT		= 0x0004,
	Interrupt_PLATFORM	= 0x0008,
	Interrupt_VERSION	= 0x0010,
	Interrupt_FREQUENCY = 0x0020,
	Interrupt_SOFTWARE1	= 0x0040,
	Interrupt_SOFTWARE2	= 0x0080,
	Interrupt_FIC		= 0x0100,
	Interrupt_MSC		= 0x0200,
	Interrupt_MCISI		= 0x0400
} Interrupt;


/**
 * The type defination of Multiplier.
 */
typedef enum {
	Multiplier_1X = 0,
	Multiplier_2X
} Multiplier;


/**
 * The type defination of StreamType.
 */
typedef enum {
	StreamType_NONE = 0,		// Invalid (Null) StreamType
	StreamType_DVBH_DATAGRAM,
	StreamType_DVBH_DATABURST,
	StreamType_DVBT_DATAGRAM,
	StreamType_DVBT_PARALLEL,
	StreamType_DVBT_SERIAL,
	StreamType_TDMB_DATAGRAM,
	StreamType_FM_DATAGRAM,
	StreamType_FM_I2S
} StreamType;


/**
 * The type defination of StreamType.
 */
typedef enum {
	Architecture_NONE = 0,		// Inavalid (Null) Architecture
	Architecture_DCA,
	Architecture_PIP
} Architecture;


/**
 * The type defination of ClockTable.
 */
typedef struct {
	Dword crystalFrequency;
	Dword adcFrequency;
} ClockTable;


/**
 * The type defination of SnrTable.
 */
typedef struct {
	Dword errorCount;
	Dword snr;
	double errorRate;
} SnrTable;


/**
 * The type defination of MeanTable.
 */
typedef struct {

	Dword mean;
	Dword errorCount;
} MeanTable;


/**
 * The type defination of Polarity.
 */
typedef enum {
    Polarity_NORMAL = 0,
    Polarity_INVERSE
} Polarity;


/**
 * The type defination of Processor.
 */
typedef enum {
    Processor_LINK = 0,
    Processor_OFDM = 8
} Processor;


/**
 * The type defination of Product.
 */
typedef enum {
	Product_GANYMEDE = 0,
    Product_JUPITER,
    Product_GEMINI,
} Product;


/**
 * The type defination of BurstSize.
 */
typedef enum {
	BurstSize_1024 = 0,
	BurstSize_2048,
	BurstSize_4096
} BurstSize;


/**
 * The type defination of PidTable.
 */
typedef struct {
	Word pid[32];
} PidTable;


/**
 * The type defination of Demodulator.
 */
typedef struct {
	Product product;
	Handle userData;
	Handle driver;
} Demodulator;


//#include "user.h"


/**
 * The type defination of Statistic.
 */
typedef struct {
    Bool signalPresented;       /** Signal is presented.                                                                         */
    Bool signalLocked;          /** Signal is locked.                                                                            */
    Byte signalQuality;         /** Signal quality, from 0 (poor) to 100 (good).                                                 */
    Byte signalStrength;        /** Signal strength from 0 (weak) to 100 (strong).                                               */
} Statistic;


/**
 * The type defination of Statistic.
 */
typedef struct {
    Word abortCount;
    Byte postVitBitCount;
    Byte postVitErrorCount;
    /** float point */
    Byte softBitCount;
    Byte softErrorCount;
    Byte preVitBitCount;
    Byte preVitErrorCount;
    //double snr;
} ChannelStatistic;



/**
 * The type defination of SubchannelStatistic.
 */
typedef struct {
	Word abortCount;
	Dword postVitBitCount;
	Dword postVitErrorCount;
	Word ficCount;				// Total FIC error count
	Word ficErrorCount;			// Total FIC count
#if User_FLOATING_POINT
	Dword softBitCount;
	Dword softErrorCount;
	Dword preVitBitCount;
	Dword preVitErrorCount;
	double snr;
#endif
} SubchannelStatistic;


/**
 * The type defination of AgcVoltage.
 */
#if User_FLOATING_POINT
typedef struct {
    double doSetVolt;
    double doPuUpVolt;
} AgcVoltage;
#endif


/**
 * General demodulator register-write function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be written.
 * @param bufferLength number, 1-8, of registers to be written.
 * @param buffer buffer used to store values to be written to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*WriteRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Dword			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Dword			writeBufferLength,
	IN  Byte*			writeBuffer
);


/**
 * General demodulator register-read function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*WriteScatterRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Byte			valueSetsAddressLength,
	IN  Byte			valueSetsLength,
	IN  ValueSet*		valueSets
);


/**
 * General tuner register-write function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be written.
 * @param bufferLength number, 1-8, of registers to be written.
 * @param buffer buffer used to store values to be written to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*WriteTunerRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Byte			tunerAddress,
	IN  Word			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Byte			writeBufferLength,
	IN  Byte*			writeBuffer
);


/**
 * General write EEPROM function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be written.
 * @param buffer buffer used to store values to be written to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*WriteEepromValues) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Byte			eepromAddress,
	IN	Word			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Byte			writeBufferLength,
	IN	Byte*			writeBuffer
);


/**
 * General demodulator register-read function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ReadRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Dword			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Dword			readBufferLength,
	OUT Byte*			readBuffer
);


/**
 * General demodulator register-read function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ReadScatterRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Byte			valueSetsAddressLength,
	IN  Byte			valueSetsLength,
	OUT ValueSet*		valueSets
);


/**
 * General tuner register-read function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ReadTunerRegisters) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Byte			tunerAddress,
	IN  Word			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Byte			readBufferLength,
	IN  Byte*			readBuffer
);


/**
 * General read EEPROM function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ReadEepromValues) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Byte			eepromAddress,
	IN	Word			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Byte			readBufferLength,
	OUT	Byte*			readBuffer
);


/**
 * General demodulator register-read function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ModifyRegister) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Dword			registerAddress,
	IN  Byte			registerAddressLength,
	IN  Byte			position,
	IN  Byte			length,
	IN  Byte			value
);


/**
 * General load firmware function
 *
 * @param demodulator the handle of demodulator.
 * @param length The length of firmware.
 * @param firmware The byte array of firmware.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*LoadFirmware) (
	IN  Demodulator*	demodulator,
	IN  Dword			firmwareLength,
	IN  Byte*			firmware
);


/**
 * General reboot function
 *
 * @param demodulator the handle of demodulator.
 * @param length The length of firmware.
 * @param firmware The byte array of firmware.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*Reboot) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip
);


/**
 * General send command function
 *
 * @param demodulator the handle of demodulator.
 * @param command The command which you wan.
 * @param valueLength value length.
 * @param valueBuffer value buffer.
 * @param referenceLength reference length.
 * @param referenceBuffer reference buffer.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*SendCommand) (
	IN  Demodulator*	demodulator,
	IN  Word			command,
	IN  Byte			chip,
	IN  Processor		processor,
	IN  Byte			writeBufferLength,
	IN  Byte*			writeBuffer,
	IN  Byte			readBufferLength,
	OUT Byte*			readBuffer
);


/**
 * General read EEPROM function
 *
 * @param demodulator the handle of demodulator.
 * @param registerAddress address of register to be read.
 * @param bufferLength number, 1-8, of registers to be read.
 * @param buffer buffer used to store values to be read to specified 
 *        registers.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ReceiveData) (
	IN  Demodulator*	demodulator,
	IN	Dword			registerAddress,
	IN  Dword			readBufferLength,
	OUT	Byte*			readBuffer
);


/**
 * The type defination of BusDescription
 */
typedef struct {
	WriteRegisters			writeRegisters;
	WriteScatterRegisters	writeScatterRegisters;
	WriteTunerRegisters		writeTunerRegisters;
	WriteEepromValues		writeEepromValues;
	ReadRegisters			readRegisters;
	ReadScatterRegisters	readScatterRegisters;
	ReadTunerRegisters		readTunerRegisters;
	ReadEepromValues		readEepromValues;
	ModifyRegister			modifyRegister;
	LoadFirmware			loadFirmware;
	Reboot					reboot;
	SendCommand				sendCommand;
	ReceiveData				receiveData;
} BusDescription;


/**
 * General tuner opening function
 *
 * @param demodulator the handle of demodulator.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*OpenTuner) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip
);


/**
 * General tuner closing function
 *
 * @param demodulator the handle of demodulator.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*CloseTuner) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip
);


/**
 * General tuner setting function
 *
 * @param demodulator the handle of demodulator.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*SetTuner) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Word			bandwidth,
	IN  Dword			frequency
);


/**
 * General tuner adjusting function
 *
 * @param demodulator the handle of demodulator.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*AdjustTuner) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN  Bool			strong
);


/**
 * The type defination of TunerDescription
 */
typedef struct {
	OpenTuner		openTuner;
	CloseTuner		closeTuner;
    SetTuner		setTuner;
	AdjustTuner		adjustTuner;
    ValueSet*		tunerScript;
    Word			tunerScriptLength;
    Byte			tunerAddress;
	Long*			strengthTable;
    Byte			registerAddressLength;
    Dword			ifFrequency;
    Bool			inversion;
} TunerDescription;


/**
 * General demodulator stream type entrance function
 *
 * @param demodulator the handle of demodulator.
 * @param currentType current stream type.
 * @param nextType next stream type.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*EnterStreamType) (
	IN  Demodulator*	demodulator,
	IN	StreamType		currentType,
	IN	StreamType		nextType
);


/**
 * General demodulator stream type leaving function
 *
 * @param demodulator the handle of demodulator.
 * @param currentType current stream type.
 * @param nextType next stream type.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*LeaveStreamType) (
	IN  Demodulator*	demodulator,
	IN	StreamType		currentType,
	IN	StreamType		nextType
);


/**
 * General demodulator acquire channel function.
 *
 * @param demodulator the handle of demodulator.
 * @param chip The index of demodulator. The possible values are 
 *        0~7.
 * @param bandwidth The channel bandwidth. 
 *        DVB-T: 5000, 6000, 7000, and 8000 (KHz).
 *        DVB-H: 5000, 6000, 7000, and 8000 (KHz).
 *        T-DMB: 5000, 6000, 7000, and 8000 (KHz).
 *        FM: 100, and 200 (KHz).
 * @param frequency the channel frequency in KHz.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*AcquireChannel) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	IN	Word			bandwidth,
	IN  Dword			frequency
);


/**
 * General demodulator get lock status.
 *
 * @param demodulator the handle of demodulator.
 * @param chip The index of demodulator. The possible values are 
 *        0~7.
 * @param locked the result of frequency tuning. True if there is 
 *        demodulator can lock signal, False otherwise.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*IsLocked) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	OUT Bool*			locked
);


/**
 * Get the statistic values of demodulator, it includes Pre-Viterbi BER,
 * Post-Viterbi BER, Abort Count, Signal Presented Flag, Signal Locked Flag,
 * Signal Quality, Signal Strength, Delta-T for DVB-H time slicing.
 *
 * @param demodulator the handle of demodulator.
 * @param chip The index of demodulator. The possible values are 
 *        0~7.
 * @param statistic the structure that store all statistic values.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*GetStatistic) (
	IN  Demodulator*	demodulator,
	IN  Byte			chip,
	OUT Statistic*		statistic
);


/**
 * General demodulator get interrupt status.
 *
 * @param demodulator the handle of demodulator.
 * @param interrupts the type of interrupts.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*GetInterrupts) (
	IN  Demodulator*	demodulator,
	OUT Interrupts*		interrupts
);


/**
 * General demodulator clear interrupt status.
 *
 * @param demodulator the handle of demodulator.
 * @param interrupt interrupt name.
 * @param packetUnit the number of packet unit for Post-Viterbi.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*ClearInterrupt) (
	IN  Demodulator*	demodulator,
	IN  Interrupt		interrupt
);


/**
 * General demodulator get the data length
 * NOTE: data can't be transfer via I2C bus, in order to transfer data
 * host must provide SPI bus.
 *
 * @param demodulator the handle of demodulator.
 * @param dataLength the length of data.
 * @param valid True if the data length is valid.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*GetDataLength) (
	IN  Demodulator*	demodulator,
	OUT Dword*			dataLength,
	OUT Bool*			valid
);


/**
 * General demodulator get data.
 *
 * @param demodulator the handle of demodulator.
 * @param bufferLength the length of buffer.
 * @param buffer buffer used to get Data.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 */
typedef Dword (*GetData) (
	IN  Demodulator*	demodulator,
	IN  Dword			bufferLength,
	OUT Byte*			buffer
);


/**
 * Get datagram from device.
 *
 * @param demodulator the handle of demodulator.
 * @param bufferLength the number of registers to be read.
 * @param buffer a byte array which is used to store values to be read.
 * @return Error_NO_ERROR: successful, non-zero error code otherwise.
 * @return Error_BUFFER_INSUFFICIENT: if buffer is too small.
 */
typedef Dword (*GetDatagram) (
	IN  Demodulator*	demodulator,
	OUT Dword*			bufferLength,
	OUT Byte*			buffer
);


/**
 * The type defination of StandardDescription
 */
typedef struct {
	EnterStreamType		enterStreamType;
	LeaveStreamType		leaveStreamType;
	AcquireChannel		acquireChannel;
	IsLocked			isLocked;
	GetStatistic		getStatistic;
	GetInterrupts		getInterrupts;
	ClearInterrupt		clearInterrupt;
	GetDataLength		getDataLength;
	GetData				getData;
	GetDatagram			getDatagram;
} StandardDescription;


/**
 * The data structure of DefaultDemodulator
 */
typedef struct {
	// Basic structure
	Product product;
	Handle userData;
	Handle driver;
	Dword options;
	Word busId;
	BusDescription busDescription;
	Word tunerId;
	TunerDescription tunerDescription;
	Dword firmwareCodeLength;
	Byte* firmwareCodes;
	Dword firmwareSegmentLength;
	Segment* firmwareSegments;
	Dword firmwarePartitionLength;
	Byte* firmwarePartitions;
	Word scriptLength;
	ValueSet* script;
	Byte chipNumber;
	Word sawBandwidth;
	Dword crystalFrequency;
	Dword adcFrequency;
	StreamType streamType;
	Architecture architecture;
	Word bandwidth;
	Dword frequency;
	Dword fcw;
	Byte shiftIndex;
	Byte exShiftIndex;
	Byte unplugThreshold;
	Bool statisticPaused;
	Bool statisticUpdated;
	Statistic statistic[2];
	ChannelStatistic channelStatistic[2];
	Byte strongSignal[2];
	Byte hostInterface[2];
	Bool booted;
	Bool initialized;
} DefaultDemodulator;


/**
 * The data structure of Ganymede
 */
typedef struct {
	// Basic structure
	Product product;
	Handle userData;
	Handle driver;
	Dword options;
	Word busId;
	BusDescription busDescription;
	Word tunerId;
	TunerDescription tunerDescription;
	Dword firmwareCodeLength;
	Byte* firmwareCodes;
	Dword firmwareSegmentLength;
	Segment* firmwareSegments;
	Dword firmwarePartitionLength;
	Byte* firmwarePartitions;
	Word scriptLength;
	ValueSet* script;
	Byte chipNumber;
	Word sawBandwidth;
	Dword crystalFrequency;
	Dword adcFrequency;
	StreamType streamType;
	Architecture architecture;
	Word bandwidth;
	Dword frequency;
	Dword fcw;
	Byte shiftIndex;
	Byte exShiftIndex;
	Byte unplugThreshold;
	Bool statisticPaused;
	Bool statisticUpdated;
	Statistic statistic[2];
	ChannelStatistic channelStatistic[2];
	Byte strongSignal[2];
	Byte hostInterface[2];
	Bool booted;
	Bool initialized;

	// DVB-T structure
	Bool dataReady;
	Byte pidCounter;
	PidTable pidTable[2];
	BurstSize burstSize;
	StandardDescription dvbtStandardDescription;
} Ganymede;


/**
 * The data structure of Jupiter
 */
typedef struct {
	// Basic structure
	Product product;
	Handle userData;
	Handle driver;
	Dword options;
	Word busId;
	BusDescription busDescription;
	Word tunerId;
	TunerDescription tunerDescription;
	Dword firmwareCodeLength;
	Byte* firmwareCodes;
	Dword firmwareSegmentLength;
	Segment* firmwareSegments;
	Dword firmwarePartitionLength;
	Byte* firmwarePartitions;
	Word scriptLength;
	ValueSet* script;
	Byte chipNumber;
	Word sawBandwidth;
	Dword crystalFrequency;
	Dword adcFrequency;
	StreamType streamType;
	Architecture architecture;
	Word bandwidth;
	Dword frequency;
	Dword fcw;
	Byte shiftIndex;
	Byte exShiftIndex;
	Byte unplugThreshold;
	Bool statisticPaused;
	Bool statisticUpdated;
	Statistic statistic[2];
	ChannelStatistic channelStatistic[2];
	Byte strongSignal[2];
	Byte hostInterface[2];
	Bool booted;
	Bool initialized;

	// DVB-T structure
	Bool dataReady;
	Byte pidCounter;
	PidTable pidTable[2];
	BurstSize burstSize;
	StandardDescription dvbtStandardDescription;

	// DVB-H structure
	Bool platformReady;
	Bool versionReady;
	Bool sipsiReady;
	Byte ipCounter;
	Platform platform;
	Bool activeNext;
	StandardDescription dvbhStandardDescription;
} Jupiter;


/**
 * The data structure of Gemini
 */
typedef struct {
	// Basic structure
	Product product;
	Handle userData;
	Handle driver;
	Dword options;
	Word busId;
	BusDescription busDescription;
	Word tunerId;
	TunerDescription tunerDescription;
	Dword firmwareCodeLength;
	Byte* firmwareCodes;
	Dword firmwareSegmentLength;
	Segment* firmwareSegments;
	Dword firmwarePartitionLength;
	Byte* firmwarePartitions;
	Word scriptLength;
	ValueSet* script;
	Byte chipNumber;
	Word sawBandwidth;
	Dword crystalFrequency;
	Dword adcFrequency;
	StreamType streamType;
	Architecture architecture;
	Word bandwidth;
	Dword frequency;
	Dword fcw;
	Byte shiftIndex;
	Byte exShiftIndex;
	Byte unplugThreshold;
	Bool statisticPaused;
	Bool statisticUpdated;
	Statistic statistic[2];
	ChannelStatistic channelStatistic[2];
	Byte strongSignal[2];
	Byte hostInterface[2];
	Bool booted;
	Bool initialized;

	// DVB-T structure
	Bool dataReady;
	Byte pidCounter;
	PidTable pidTable[2];
	BurstSize burstSize;
	StandardDescription dvbtStandardDescription;

	// DVB-H structure
	Bool platformReady;
	Bool versionReady;
	Bool sipsiReady;
	Byte ipCounter;
	Platform platform;
	Bool activeNext;
	StandardDescription dvbhStandardDescription;

	// T-DMB structure
	Bool ficReady;
	Bool figReady;
	Bool mcisiReady;
	Bool group1Ready;
	Bool group2Ready;
	Bool group3Ready;
	Word ficLength[2];
	Word dataLength[4];
	Service service;
	SubchannelStatistic subchannelStatistic[2];
	StandardDescription tdmbStandardDescription;

	// FM structure
	StandardDescription fmStandardDescription;
} Gemini;


extern const Byte Standard_bitMask[8];
#define REG_MASK(pos, len)                (Standard_bitMask[len-1] << pos)
#define REG_CLEAR(temp, pos, len)         (temp & (~REG_MASK(pos, len)))
#define REG_CREATE(val, temp, pos, len)   ((val << pos) | (REG_CLEAR(temp, pos, len)))
#define REG_GET(value, pos, len)          ((value & REG_MASK(pos, len)) >> pos)
#define LOWBYTE(w)		((Byte)((w) & 0xff))
#define HIGHBYTE(w)		((Byte)((w >> 8) & 0xff))

#endif

#pragma once

#include <QtCore/qsystemdetection.h>

#if defined (Q_OS_LINUX)

#include <sys/ioctl.h>

namespace nx::vms::server::nvr::hanwha {

#define NX_HANWHA_CMDID_NRBITS 8
#define NX_HANWHA_CMDID_SUBGBITS 4
#define NX_HANWHA_CMDID_RSVD1BITS 4
#define NX_HANWHA_CMDID_GRPBITS	8
#define NX_HANWHA_CMDID_USGBITS	4

#define NX_HANWHA_CMDID_NRMASK ((1 << NX_HANWHA_CMDID_NRBITS) - 1)
#define NX_HANWHA_CMDID_SUBGMASK ((1 << NX_HANWHA_CMDID_SUBGBITS) - 1)
#define NX_HANWHA_CMDID_GRPMASK	((1 << NX_HANWHA_CMDID_GRPBITS) - 1)
#define NX_HANWHA_CMDID_USGMASK	((1 << NX_HANWHA_CMDID_USGBITS) - 1)

#define NX_HANWHA_CMDID_NRSHIFT	0
#define NX_HANWHA_CMDID_SUBGSHIFT (NX_HANWHA_CMDID_NRSHIFT + NX_HANWHA_CMDID_NRBITS)
#define NX_HANWHA_CMDID_GRPSHIFT (NX_HANWHA_CMDID_SUBGSHIFT + \
    NX_HANWHA_CMDID_SUBGBITS + NX_HANWHA_CMDID_RSVD1BITS)
#define NX_HANWHA_CMDID_USGSHIFT (NX_HANWHA_CMDID_GRPSHIFT + NX_HANWHA_CMDID_GRPBITS)

#define NX_HANWHA_MAKECMDID(usage, group, subgroup, number) \
    ((usage << NX_HANWHA_CMDID_USGSHIFT) | (group << NX_HANWHA_CMDID_GRPSHIFT) | \
    (subgroup << NX_HANWHA_CMDID_SUBGSHIFT) | (number << NX_HANWHA_CMDID_NRSHIFT))

#define NX_HANWHA_CMDID_USG_COMMON 0x00
#define NX_HANWHA_CMDID_USG_IP1829 0x01
#define NX_HANWHA_CMDID_USG_DBS	0x02
#define NX_HANWHA_CMDID_GRP_BASIC 0x01
#define NX_HANWHA_CMDID_SUBG_SMI 0x01

static constexpr int kSetPortPoweringStatusCommand = 0x1;
static constexpr int kGetPortPoweringStatusCommand = 0x2;
static constexpr int kGetPortLinkStatusCommand = 0x3;
static constexpr int kGetPortMacAddressCommand = 0x4;
static constexpr int kGetPortSpeedCommand = NX_HANWHA_MAKECMDID(
    NX_HANWHA_CMDID_USG_COMMON,
    NX_HANWHA_CMDID_GRP_BASIC,
    NX_HANWHA_CMDID_SUBG_SMI,
    3);

static constexpr int kGetPortPowerConsumptionCommand = 0xd;
static constexpr int kGetBoardIdCommand = 0xe;
static constexpr int kReadI2cCommand = 0xf;

static constexpr int kI2cDeviceClassRegister = 0x14;

static constexpr int kSetRecordingLedCommand = 0x34;
static constexpr int kSetAlarmOutputLedCommand = 0x33;
static constexpr int kSetPoeOverBudgetLedCommand = 0x32;

static constexpr int kSetAlarmOutput0Command = 0x01;
static constexpr int kSetAlarmOutput1Command = 0x02;

static constexpr int kGetAlarmInput0Command = 0x11;
static constexpr int kGetAlarmInput1Command = 0x12;
static constexpr int kGetAlarmInput2Command = 0x13;
static constexpr int kGetAlarmInput3Command = 0x14;

static constexpr int kSetBuzzerCommand = 0x30;
static constexpr int kGetFanAlarmCommand = 0x43;

// TODO: #dmishin figure out why there are two ways of retrieving board id.
#define RES_GET_BOARDID			0x20

// ------------------------------------------------------------------------------------------------

template<typename CommandData>
int prepareCommand(int commandId)
{
    return _IOC(_IOC_READ | _IOC_WRITE, commandId, 0, sizeof(CommandData));
}

// ------------------------------------------------------------------------------------------------

struct LinkStatusCommandData
{
    int portNumber = 0;
    int linkStatus = 0;
};

struct PortMacAddressCommandData
{
    int index = 0;
    int block = 0;
    int portNumber = 0;
    unsigned char macAddress[6];
};

struct PortPowerConsumptionCommandData
{
    int portNumber = 0;
    int voltageLsb = 0;
    int voltageMsb = 0;
    int currentLsb = 0;
    int currentMsb = 0;
};

struct PortPoweringStatusCommandData
{
    int portNumber = 0;
    int poweringStatus = 0;
};

// TODO: #dmishin figure out real structure for this command.
struct PortSpeedCommandData
{
    int portNumber = 0;
    int portSpeed = 0;
};

struct I2cCommandData
{
    unsigned char deviceAddress = 0;
    unsigned int registerAddress = 0;
    unsigned int registerAddressSize = 0;
    unsigned int data = 0;
    unsigned int dataSize = 0;
};

} // namespace nx::vms::server::nvr::hanwha

#endif

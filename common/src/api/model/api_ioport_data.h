#pragma once

#include <common/common_globals.h>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnIOPortData
{
    QnIOPortData(): portType(Qn::PT_Disabled), iDefaultState(Qn::IO_OpenCircuit), oDefaultState(Qn::IO_OpenCircuit), autoResetTimeoutMs(0) {}

    QString getName() const                    { return portType == Qn::PT_Output ? outputName : inputName; }
    Qn::IODefaultState getDefaultState() const { return portType == Qn::PT_Output ? oDefaultState : iDefaultState; }

    QString id;
    Qn::IOPortType portType;
    Qn::IOPortTypes supportedPortTypes;
    QString inputName;
    QString outputName;
    Qn::IODefaultState iDefaultState;
    Qn::IODefaultState oDefaultState;
    int autoResetTimeoutMs; // for output only. Keep output state on during timeout if non zero
};
typedef std::vector<QnIOPortData> QnIOPortDataList;
#define QnIOPortData_Fields (id)(portType)(supportedPortTypes)(inputName)(outputName)(iDefaultState)(oDefaultState)(autoResetTimeoutMs)

Q_DECLARE_METATYPE(QnIOPortData)

struct QnCameraPortsData
{
    QnUuid id;
    QnIOPortDataList ports;
};
#define QnCameraPortsData_Fields (id)(ports)

struct QnIOStateData
{
    QnIOStateData(): isActive(false), timestamp(0) {}
    QnIOStateData(const QString& id, bool isActive, qint64 timestamp): id(id), isActive(isActive), timestamp(timestamp) {}

    QString id;
    bool isActive;
    qint64 timestamp;
};

typedef std::vector<QnIOStateData> QnIOStateDataList;
#define QnIOStateData_Fields (id)(isActive)(timestamp)

Q_DECLARE_METATYPE(QnIOStateData)

struct QnCameraIOStateData
{
    QnUuid id;
    QnIOStateDataList state;
};
typedef std::vector<QnCameraIOStateData> QnCameraIOStateDataList;
#define QnCameraIOStateData_Fields (id)(state)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnIOPortData)(QnCameraPortsData)(QnIOStateData)(QnCameraIOStateData), (json)(ubjson)(eq))

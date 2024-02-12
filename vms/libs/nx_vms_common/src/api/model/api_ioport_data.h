// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

/*
 * Camera output port can contains special constant in its name. These ports can be handled in
 * camera driver in non-standard way. Also, client may display separate UI controls for such ports.
 */
NX_REFLECTION_ENUM_CLASS(ExtendedCameraOutput,
    none = 0x0,
    heater = 1 << 0,
    fan =  1 << 1,
    wiper =  1 << 2,
    autoTracking =  1 << 3,
    powerRelay =  1 << 4
);
Q_DECLARE_FLAGS(ExtendedCameraOutputs, ExtendedCameraOutput)

} // namespace nx::vms::api

struct NX_VMS_COMMON_API QnIOPortData
{
    QnIOPortData();

    QString toString() const;
    QString getName() const;

    nx::vms::api::ExtendedCameraOutput extendedCameraOutput() const;

    Qn::IODefaultState getDefaultState() const;

    QString id;
    Qn::IOPortType portType;
    Qn::IOPortTypes supportedPortTypes;
    QString inputName;
    QString outputName;
    Qn::IODefaultState iDefaultState;
    Qn::IODefaultState oDefaultState;
    int autoResetTimeoutMs; // for output only. Keep output state on during timeout if non zero

    bool operator==(const QnIOPortData& other) const = default;
};

QString toString(const QnIOPortData& portData);

typedef std::vector<QnIOPortData> QnIOPortDataList;
#define QnIOPortData_Fields (id)(portType)(supportedPortTypes)(inputName)(outputName)(iDefaultState)(oDefaultState)(autoResetTimeoutMs)

NX_REFLECTION_INSTRUMENT(QnIOPortData, QnIOPortData_Fields)


struct NX_VMS_COMMON_API QnCameraPortsData
{
    nx::Uuid id;
    QnIOPortDataList ports;
};
#define QnCameraPortsData_Fields (id)(ports)

struct NX_VMS_COMMON_API QnIOStateData
{
    QnIOStateData(): isActive(false), timestamp(0) {}
    QnIOStateData(const QString& id, bool isActive, qint64 timestamp): id(id), isActive(isActive), timestamp(timestamp) {}

    QString id;
    bool isActive = false;
    qint64 timestamp = -1;

    bool operator==(const QnIOStateData& other) const = default;
};

inline bool operator<(const QnIOStateData& lhs, const QnIOStateData& rhs)
{
    return lhs.id < rhs.id;
}

inline QString toString(const QnIOStateData& ioStateData)
{
    return QString("{id: %1, isActive: %2, timestamp: %3}")
        .arg(ioStateData.id).arg(ioStateData.isActive).arg(ioStateData.timestamp);
}

typedef std::vector<QnIOStateData> QnIOStateDataList;
#define QnIOStateData_Fields (id)(isActive)(timestamp)

struct NX_VMS_COMMON_API QnCameraIOStateData
{
    nx::Uuid id;
    QnIOStateDataList state;
};
typedef std::vector<QnCameraIOStateData> QnCameraIOStateDataList;
#define QnCameraIOStateData_Fields (id)(state)

QN_FUSION_DECLARE_FUNCTIONS(QnIOPortData, (json) (ubjson), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraPortsData, (json)(ubjson), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnIOStateData, (json)(ubjson), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraIOStateData, (json)(ubjson), NX_VMS_COMMON_API)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api_ioport_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/lexical.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnIOPortData, (ubjson)(json), QnIOPortData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraPortsData, (ubjson)(json), QnCameraPortsData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnIOStateData, (ubjson)(json), QnIOStateData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraIOStateData, (ubjson)(json), QnCameraIOStateData_Fields)

QnIOPortData::QnIOPortData()
    :
    portType(Qn::PT_Disabled),
    iDefaultState(Qn::IO_OpenCircuit),
    oDefaultState(Qn::IO_OpenCircuit),
    autoResetTimeoutMs(0)
{
}

QString toString(const QnIOPortData& ioPortData)
{
    return QJson::serialized(ioPortData);
}

QString QnIOPortData::getName() const
{
    return portType == Qn::PT_Output ? outputName : inputName;
}

Qn::IODefaultState QnIOPortData::getDefaultState() const
{
    return portType == Qn::PT_Output ? oDefaultState : iDefaultState;
}

nx::vms::api::ExtendedCameraOutput QnIOPortData::extendedCameraOutput() const
{
    if (portType != Qn::PT_Output)
        return nx::vms::api::ExtendedCameraOutput::none;

    bool success;
    if (outputName.toInt(&success); success)
        return nx::vms::api::ExtendedCameraOutput::none;

    return nx::reflect::fromString<nx::vms::api::ExtendedCameraOutput>(
        outputName.toStdString(), nx::vms::api::ExtendedCameraOutput::none);
}

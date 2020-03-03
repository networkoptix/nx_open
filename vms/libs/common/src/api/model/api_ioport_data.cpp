#include "api_ioport_data.h"

#include <nx/fusion/serialization/json.h>

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

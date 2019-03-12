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

QString QnIOPortData::toString() const
{
    return QJson::serialized(*this);
}

QString QnIOPortData::getName() const
{
    return portType == Qn::PT_Output ? outputName : inputName;
}

Qn::IODefaultState QnIOPortData::getDefaultState() const
{
    return portType == Qn::PT_Output ? oDefaultState : iDefaultState;
}

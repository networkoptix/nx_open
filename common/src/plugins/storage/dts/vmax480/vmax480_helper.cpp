#include "vmax480_helper.h"

static const char PARAM_DELIMITER = '\\';

bool QnVMax480Helper::isFullMessage(const QByteArray& message)
{
    return message.size() >= 6 && message.indexOf("\n\n") > 0;
}

bool QnVMax480Helper::deserializeCommand(const QByteArray& ba, MServerCommand* command, quint8* sequence, VMaxParamList* params)
{
    if (ba.size() < 6)
        return false;

    params->clear();
    *sequence = ba.data()[0];
    *command = (MServerCommand) ba.data()[1];
    QList<QByteArray> lines = QByteArray::fromRawData(ba.data()+4, ba.size()-4).split('\n');
    for (int i = 0; i < lines.size(); ++i)
    {
        QList<QByteArray> tmp = lines[i].split(PARAM_DELIMITER);
        if (tmp.size() >= 2)
            params->insert(tmp[0], tmp[1]);
    }

    return true;
}

QByteArray QnVMax480Helper::serializeCommand(MServerCommand command, quint8 sequence, const VMaxParamList& params)
{
    QByteArray result;
    quint8 commandHeader[4];
    commandHeader[0] = sequence;
    commandHeader[1] = command;
    commandHeader[2] = 0;
    commandHeader[3] = 0;
    result.append((const char*) commandHeader, sizeof(commandHeader));

    for (VMaxParamList::const_iterator param = params.constBegin(); param != params.constEnd(); ++param)
    {
        result.append(param.key());
        result.append(PARAM_DELIMITER);
        result.append(param.value());
        result.append('\n');
    }
    result.append('\n');

    return result;
}

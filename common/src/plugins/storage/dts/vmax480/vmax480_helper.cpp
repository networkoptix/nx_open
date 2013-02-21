#include "vmax480_helper.h"

static const char PARAM_DELIMITER('\\');

bool QnVMax480Helper::isFullMessage(const QByteArray& message)
{
    return message.indexOf("\n\n") >= 0;
}

QByteArray QnVMax480Helper::serializeCommand(MServerCommand command, quint8 sequence, const VMaxParamList& params)
{
    quint8 header[4];
    header[0] = sequence;
    header[1] = (quint8) command;
    header[2] = header[3] = 0; // reserved for future use

    QByteArray result;
    result.append((const char*) header, sizeof(header));
    for (VMaxParamList::const_iterator itr = params.constBegin(); itr != params.constEnd(); ++itr)
    {
        result.append(itr.key());
        result.append(PARAM_DELIMITER);
        result.append(itr.value());
        result.append("\n");
    }
    result.append("\n");

    return result;
}

bool QnVMax480Helper::deserializeCommand(const QByteArray& data, MServerCommand* command, quint8* sequence, VMaxParamList* params)
{
    if (!isFullMessage(data) || data.size() < 6)
        return false;

    *sequence = (quint8) data.data()[0];
    *command = (MServerCommand) data.data()[1];

    QList<QByteArray> lines = QByteArray::fromRawData(data.data()+4, data.size()-4).split('\n');
    for (int i = 0; i < lines.size(); ++i) 
    {
        QList<QByteArray> tmp = lines[i].split(PARAM_DELIMITER);
        if (tmp.size() > 1)
            params->insert(tmp[0], tmp[1]);
    }

    return true;
}

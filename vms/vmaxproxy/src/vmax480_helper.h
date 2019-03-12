#ifndef __VMAX480_HELPER_H_
#define __VMAX480_HELPER_H_

#include <QMap>
#include <QByteArray>

typedef QMap<QByteArray, QByteArray> VMaxParamList;

enum MServerCommand {
    Command_OpenLive,
    Command_OpenArchive,
    Command_ArchivePlay,
    Command_AddChannel,
    Command_RemoveChannel,
    Command_PlayPoints,
    Command_RecordedMonth,
    Command_RecordedTime,
    Command_GetRange,
    Command_CloseConnect
};

enum VMaxDataType {
    VMAXDT_GotVideoPacket,
    VMAXDT_GotAudioPacket,
    VMAXDT_GotArchiveRange,
    VMAXDT_GotMonthInfo,
    VMAXDT_GotDayInfo
};

enum VMaxVideoCodec {
    CODEC_VSTREAM_H264 ,
    CODEC_VSTREAM_JPEG ,
    CODEC_VSTREAM_MPEG4
};

enum VMaxAudioCodec{
    CODEC_ASTREAM_MULAW ,
    CODEC_ASTREAM_G723,
    CODEC_ASTREAM_IMAACPCM,
    CODEC_ASTREAM_MSADPCM,
    CODEC_ASTREAM_PCM,
    CODEC_ASTREAM_CG726,
    CODEC_ASTREAM_CG711A,
    CODEC_ASTREAM_CG711U
};

static const char VMAX_PARAM_DELIMITER = '\\';

static const int VMAX_MAX_CH = 16;
static const int VMAX_SLICE_OF_HOUR = 60;
static const int VMAX_MAX_SLICE_DAY = VMAX_SLICE_OF_HOUR*25;

class QnVMax480Helper {
public:
    static bool deserializeCommand(const QByteArray& ba, MServerCommand* command, quint8* sequence, VMaxParamList* params)
    {
        if (ba.size() < 6)
            return false;

        params->clear();
        *sequence = ba.data()[0];
        *command = (MServerCommand) ba.data()[1];
        QList<QByteArray> lines = QByteArray::fromRawData(ba.data()+4, ba.size()-4).split('\n');
        for (int i = 0; i < lines.size(); ++i)
        {
            QList<QByteArray> tmp = lines[i].split(VMAX_PARAM_DELIMITER);
            if (tmp.size() >= 2)
                params->insert(tmp[0], tmp[1]);
        }

        return true;
    }

    static QByteArray serializeCommand(MServerCommand command, quint8 sequence, const VMaxParamList& params)
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
            result.append(VMAX_PARAM_DELIMITER);
            result.append(param.value());
            result.append('\n');
        }
        result.append('\n');
        if (params.isEmpty())
            result.append('\n');
        return result;
    }
};

#endif // __VMAX480_HELPER_H_

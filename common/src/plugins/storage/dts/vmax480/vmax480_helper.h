#ifndef __VMAX480_HELPER_H_
#define __VMAX480_HELPER_H_

typedef QMap<QByteArray, QByteArray> VMaxParamList;

enum MServerCommand {Command_OpenLive, Command_OpenArchive, Command_CloseConnect};
enum VMaxDataType { VMAXDT_GotVideoPacket, VMAXDT_GotAudioPacket, VMAXDT_GotArchiveRange };

enum VMaxVideoCodec { CODEC_VSTREAM_H264 , CODEC_VSTREAM_JPEG , CODEC_VSTREAM_MPEG4};
enum VMaxAudioCodec{ CODEC_ASTREAM_MULAW , CODEC_ASTREAM_G723, CODEC_ASTREAM_IMAACPCM, CODEC_ASTREAM_MSADPCM, CODEC_ASTREAM_PCM, CODEC_ASTREAM_CG726, CODEC_ASTREAM_CG711A, CODEC_ASTREAM_CG711U};

class QnVMax480Helper {
public:
    static bool isFullMessage(const QByteArray& message);

    static bool deserializeCommand(const QByteArray& ba, MServerCommand* command, quint8* sequence, VMaxParamList* params);
    static QByteArray serializeCommand(MServerCommand command, quint8 sequence, const VMaxParamList& params);
};

#endif // __VMAX480_HELPER_H_

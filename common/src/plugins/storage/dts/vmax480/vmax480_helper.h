#ifndef __VMAX_480_HELPER_H__
#define __VMAX_480_HELPER_H__

#include <QMap>
#include <QString>

enum VMaxDataType {VMAXDT_Unknown, VMAXDT_GotVideoPacket, VMAXDT_GotAudioPacket};
enum MServerCommand {Command_Unknown, Command_OpenLive, Command_OpenArchive, Command_CloseConnect, Command_ArchiveSeek, Command_GetChunks};

typedef QMap<QByteArray, QByteArray> VMaxParamList;

class QnVMax480Helper
{
public:
    static QByteArray serializeCommand(MServerCommand command, quint8 sequence, const VMaxParamList& params);
    static bool deserializeCommand(const QByteArray& data, MServerCommand* command, quint8* sequence, VMaxParamList* params);

    static bool isFullMessage(const QByteArray& message);

};

#endif // __VMAX_480_HELPER_H__

#include <QDebug>
#include <QByteArray>
#include <QList>
#include "socket.h"
#include "vmax480_reader.h"
#include "vmax480_helper.h"


static const int VMAX_CONNECTION_TIMEOUT = 1000 * 10;

QnVMax480Provider* openVMaxConnection(TCPSocket* socket, const VMaxParamList& params, quint8 sequence, bool isLive)
{
    QnVMax480Provider* result = new QnVMax480Provider(socket);
    result->connect(params, sequence, isLive);

    if (!result->waitForConnected(VMAX_CONNECTION_TIMEOUT))
    {
        qDebug() << "can not connect to VMAX server!";

        result->disconnect();
        delete result;
        result = 0;
    }

    return result;
}

static int isFullMessage(quint8* data, int msgLen)
{
    if (msgLen < 6)
        return 0;

    const QByteArray message = QByteArray::fromRawData((const char*)data + 4, msgLen - 4);
    int eofIdx = message.indexOf("\n\n");
    if (eofIdx == -1)
        return 0;
    return eofIdx + 2 + 4;
}

int main(int argc, char* argv[])
{

    if (argc != 3)
    {
        qWarning() << "This utility for internal use only!";
        qWarning() << "Usage:";
        qWarning() << "vmaxproxy <mediaServer vmax TCP port> <connection ID>";

        return -1;
    }

    //QMessageBox::warning(0, "test", "test");

    QnVMax480Provider* connection = 0;

    int port = QString(argv[1]).toInt();
    QByteArray connectionID = argv[2];

    qWarning() << "proxy started";

    TCPSocket mServerConnect;
    bool connected = mServerConnect.connect("127.0.0.1", port);
    if (!connected)
        return -1;

    qWarning() << "proxy connected";

    mServerConnect.setReadTimeOut(1000 * 10);

    mServerConnect.send(connectionID.data(), connectionID.size());

    qWarning() << "after send ID";

    quint8 buffer[1024*4];
    int bufferLen = 0;

    bool shouldTerminate = false;
    while(!shouldTerminate)
    {
        int msgLen = isFullMessage(buffer, bufferLen);
        while(!msgLen) 
        {
            QTime t;
            t.restart();
            int readed = mServerConnect.recv(buffer + bufferLen, sizeof(buffer) - bufferLen);
            if (readed < 1) 
            {
                if (!mServerConnect.isConnected()) {
                    shouldTerminate = true;
                    break;
                }
                if (connection)
                    connection->keepAlive();
                continue;
            }
            bufferLen += readed;
            msgLen = isFullMessage(buffer, bufferLen);
        };

        if (shouldTerminate)
            break;

        quint8 sequence;
        MServerCommand command;
        VMaxParamList params;
        QByteArray ba((const char*) buffer, msgLen);
        QnVMax480Helper::deserializeCommand(ba, &command, &sequence, &params);

        memmove(buffer, buffer + msgLen, bufferLen - msgLen);
        bufferLen -= msgLen;

        switch(command)
        {
            case Command_OpenLive:
                qDebug() << "before exec Command_OpenLive";
                connection = openVMaxConnection(&mServerConnect, params, sequence, true);
                break;
            case Command_OpenArchive:
                qDebug() << "before exec Command_OpenArchive";
                connection = openVMaxConnection(&mServerConnect, params, sequence, false);
                break;
            case Command_AddChannel:
                qDebug() << "before exec Command_AddChannel";
                connection->addChannel(params);
                break;
            case Command_RemoveChannel:
                qDebug() << "before exec Command_RemoveChannel";
                connection->removeChannel(params);
                break;
            case Command_RecordedMonth:
                if (connection) {
                    qDebug() << "before request month info";
                    connection->requestMonthInfo(params, sequence);
                }
                break;
            case Command_RecordedTime:
                if (connection) {
                    qDebug() << "before request day info";
                    connection->requestDayInfo(params, sequence);
                }
                break;
            case Command_GetRange:
                if (connection) {
                    qDebug() << "before request range info";
                    connection->requestRange(params, sequence);
                }
                break;
            case Command_ArchivePlay:
                if (connection) {
                    qDebug() << "before exec Command_ArchivePlay" <<  "seq=" << sequence << "speed=" << params.value("speed").toInt();
                    connection->archivePlay(params, sequence);
                }
                break;
            case Command_PlayPoints:
                if (connection) {
                    qDebug() << "before exec Command_PlayPoints" <<  "seq=" << sequence;
                    connection->pointsPlay(params, sequence);
                }
                break;
            case Command_CloseConnect:
                if (connection) {
                    connection->disconnect();
                    delete connection;
                    connection = 0;
                }
                break;
        }
        if (!connection)
            break;
    }

    if (connection) {
        connection->disconnect();
        delete connection;
        connection = 0;
    }

    mServerConnect.close();

    return 0;
}

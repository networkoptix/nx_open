#include <QDebug>
#include <QByteArray>
#include <QList>
#include "socket.h"
#include "vmax480_reader.h"
#include "vmax480_helper.h"


QnVMax480Provider* openVMaxConnection(TCPSocket* socket, const VMaxParamList& params, quint8 sequence, bool isLive)
{
    QnVMax480Provider* result = new QnVMax480Provider(socket);
    result->connect(params, sequence, isLive);
    return result;
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

    bool shouldTerminate = false;
    while(!shouldTerminate)
    {
        quint8 buffer[1024*4];
        int bufferLen = 0;

        do {
            QTime t;
            t.restart();
            int readed = mServerConnect.recv(buffer + bufferLen, sizeof(buffer) - bufferLen);
            if (readed < 1) 
            {
                if (t.elapsed() < 10) {
                    shouldTerminate = true;
                    break;
                }
                if (connection)
                    connection->keepAlive();
                continue;
            }
            bufferLen += readed;
        } while (!QnVMax480Helper::isFullMessage(QByteArray::fromRawData((const char*) buffer, bufferLen)));

        if (shouldTerminate)
            break;

        quint8 sequence;
        MServerCommand command;
        VMaxParamList params;
        QByteArray ba = QByteArray::fromRawData((const char*) buffer, bufferLen);
        QnVMax480Helper::deserializeCommand(ba, &command, &sequence, &params);

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
            case Command_ArchivePlay:
                if (connection) {
                    qDebug() << "before exec Command_ArchivePlay";
                    connection->archivePlay(params, sequence);
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
    }

    if (connection) {
        connection->disconnect();
        delete connection;
        connection = 0;
    }

    mServerConnect.close();

    return 0;
}

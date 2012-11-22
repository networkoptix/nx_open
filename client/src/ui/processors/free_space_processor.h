#ifndef FREE_SPACE_PROCESSOR_H
#define FREE_SPACE_PROCESSOR_H

struct FreeSpaceInfo
{
    FreeSpaceInfo(): freeSpace(0), usedSpace(0), errorCode(0) {}
    FreeSpaceInfo(qint64 _freeSpace, qint64 _usedSpace, int _errorCode): freeSpace(_freeSpace), usedSpace(_usedSpace), errorCode(_errorCode) {}
    qint64 freeSpace;
    qint64 usedSpace;
    int errorCode;
};
typedef QMap<int, FreeSpaceInfo> FreeSpaceMap;

class CheckFreeSpaceReplyProcessor: public QObject
{
    Q_OBJECT
public:
    static const qint64 MIN_RECORD_FREE_SPACE = 1000ll * 1000ll * 1000ll * 5;

    static const int INVALID_PATH = -1;
    static const int SERVER_ERROR = -2;

    CheckFreeSpaceReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    FreeSpaceMap freeSpaceInfo() const {
        return m_freeSpace;
    }

signals:
    void replyReceived(int status, qint64 freeSpace, qint64 usedSpace, int handle);

public slots:
    void processReply(int status, qint64 freeSpace, qint64 usedSpace,  int handle)
    {
        int errCode = status == 0 ? (freeSpace > 0 ? 0 : INVALID_PATH) : SERVER_ERROR;
        m_freeSpace.insert(handle, FreeSpaceInfo(freeSpace, usedSpace, errCode));
        emit replyReceived(status, freeSpace, usedSpace, handle);
    }

private:
    FreeSpaceMap m_freeSpace;
};

#endif // FREE_SPACE_PROCESSOR_H

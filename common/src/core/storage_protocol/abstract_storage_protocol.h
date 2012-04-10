#ifndef __STORAGE_PROTOCOL_H__
#define __STORAGE_PROTOCOL_H__

#include <QIODevice>
#include <QSharedPointer>
#include <QFileInfoList>
#include <QMap>

class QnAbstractStorageProtocol
{
public:

    QnAbstractStorageProtocol(const QString& baseUrl = "");

    virtual URLProtocol getURLProtocol() const = 0;

    
    /*
    virtual QString protocolName() const = 0;
    virtual bool open(const QString& url, QIODevice::OpenMode mode) = 0;
    virtual int read(quint8* buf, int size) = 0;
    virtual int write(quint8* buf, int size) = 0;
    virtual int seek(quint64 pos, int whence) = 0;
    virtual int close() = 0;
    */

    /*
    * Returns recommend file duration in seconds
    */
    virtual int getChunkLen() const = 0;

    virtual bool isNeedControlFreeSpace() = 0;
    virtual qint64 getFreeSpace(const QString& url) = 0;
    virtual bool removeFile(const QString& url) = 0;
    virtual bool removeDir(const QString& url) = 0;
    virtual bool isStorageAvailable(const QString& url) = 0;
    virtual QFileInfoList getFileList(const QString& dirName) = 0;
    virtual bool isFileExists(const QString& url) = 0;
    virtual bool isDirExists(const QString& url) = 0;

    QString getUrlPrefix() const;
protected:
    QString removePrefix(const QString& url);

private:
    QString m_baseUrl;

};

typedef QSharedPointer<QnAbstractStorageProtocol> QnAbstractStorageProtocolPtr;

class QnStorageProtocolManager
{
public:
    QnStorageProtocolManager();
    virtual ~QnStorageProtocolManager();
    static QnStorageProtocolManager* instance();
    QnAbstractStorageProtocolPtr getProtocol(const QString& url);
    void registerProtocol(QnAbstractStorageProtocolPtr protocol, bool isDefaultProtocol = false);
private:
    QMap<QString, QnAbstractStorageProtocolPtr> m_protocols;
    QnAbstractStorageProtocolPtr m_defaultProtocol;
    QMutex m_mutex;
};

#define qnStorageProtocolManager QnStorageProtocolManager::instance()

#endif // __STORAGE_PROTOCOL_H__

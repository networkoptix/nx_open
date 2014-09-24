#ifndef coldstore_helper_h_2137_h
#define coldstore_helper_h_2137_h

#ifdef ENABLE_COLDSTORE

#include <QtCore/QtGlobal>
#include <QtCore/QFileInfoList>
#include <QHash>
#include <QtCore/QTime>
#include <QtCore/QMutex>
#include <QSharedPointer>


struct QnCSFileInfo
{
    QnCSFileInfo():
    shift(0),
    len(0)
    {

    }

    QnCSFileInfo(qint64 shift_, qint64 len_):
    shift(shift_),
    len(len_)
    {

    };

    qint64 shift;
    qint64 len;
};

struct QnCSFile
{
    QByteArray data;
    QString fn;
};

class QnColdStoreMetaData
{
public:
    QnColdStoreMetaData();
    ~QnColdStoreMetaData();

    int age() const; // age in seconds ( last used )

    void put(const QString& fn, QnCSFileInfo info);

    // returns QnCSFileInfo with 0 len no such file
    QnCSFileInfo getFileinfo(const QString& fn) const;

    QFileInfoList fileInfoList(const QString& subPath) const;

    QByteArray toByteArray() const;
    void fromByteArray(const QByteArray& ba);

    bool needsToBesaved() const;
    void setNeedsToBesaved(bool v);



private:

    QHash<QString, QnCSFileInfo> m_hash;

    mutable QTime m_lastUsageTime;

    bool m_needsToBesaved;

    mutable QMutex m_mutex;
};

typedef QSharedPointer<QnColdStoreMetaData> QnColdStoreMetaDataPtr;
class QnPlColdStoreStorage;

class QnColdStoreMetaDataPool
{
public:
    QnColdStoreMetaDataPool(QnPlColdStoreStorage *csStorage);
    ~QnColdStoreMetaDataPool();
    QnColdStoreMetaDataPtr getStoreMetaData(const QString& csFn);
private:
    void checkIfSomedataNeedstoberemoved();
private:    
    QnPlColdStoreStorage* m_csStorage;

    QHash<QString, QnColdStoreMetaDataPtr>  m_pool;

    QMutex m_mutex;
};

class QnColdStoreConnection;

class QnCsTimeunitConnectionHelper
{
public:
    QnCsTimeunitConnectionHelper();
    ~QnCsTimeunitConnectionHelper();

    bool open(const QString& addr, const QString& csFile);

    void write(const QByteArray& ba, const QString& fn);

    void close();

    QString getCsFileName() const; 

    QnColdStoreMetaDataPtr getMD() ;

private:
    QnColdStoreConnection* m_connect;
    QnColdStoreMetaDataPtr m_metaData;

    QString m_csFileName;
    QString m_addr;

};


#endif // ENABLE_COLDSTORE

#endif //coldstore_helper_h_2137_h

#ifndef coldstore_helper_h_2137_h
#define coldstore_helper_h_2137_h


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
    QnColdStoreMetaData(const QString& csFileName);
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

    QString csFileName() const;

private:
    QString m_csFileName;

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


#endif //coldstore_helper_h_2137_h
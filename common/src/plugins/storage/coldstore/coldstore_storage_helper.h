#ifndef coldstore_helper_h_2137_h
#define coldstore_helper_h_2137_h

struct QnCSFileInfo
{
    QnCSFileInfo()
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

    void put(const QString& fn, QnCSFileInfo info);

    bool hasSuchFile(const QString& fn) const;
    QnCSFileInfo getFileinfo(const QString& fn) const;

    QByteArray toByteArray() const;
    void fromByteArray(const QByteArray& ba);

    bool needsToBesaved() const;
    void setNeedsToBesaved(bool v);

private:
    QHash<QString, QnCSFileInfo> m_hash;

    bool m_needsToBesaved;
};




#endif //coldstore_helper_h_2137_h
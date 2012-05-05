#include "coldstore_storage_helper.h"

QnColdStoreMetaData::QnColdStoreMetaData()
{
    m_needsToBesaved = false;
}

QnColdStoreMetaData::~QnColdStoreMetaData()
{
    
}

void QnColdStoreMetaData::put(const QString& fn, QnCSFileInfo info)
{
    m_hash.insert(fn, info);
}

bool QnColdStoreMetaData::hasSuchFile(const QString& fn) const
{
    return m_hash.contains(fn);
}

QnCSFileInfo QnColdStoreMetaData::getFileinfo(const QString& fn) const
{
    return m_hash.value(fn);
}

QByteArray QnColdStoreMetaData::toByteArray() const
{
    QByteArray result;
    result.reserve(m_hash.size()*66 + 4); // to avoid memory realocation

    result.append("1;"); //version

    QHashIterator<QString, QnCSFileInfo> it(m_hash);
    while (it.hasNext()) 
    {
        it.next();
        QString fn = it.key();
        QnCSFileInfo fi = it.value();

        result.append(fn);
        result.append(";");

        result.append(QString::number(fi.shift, 16));
        result.append(";");

        result.append(QString::number(fi.len, 16));
        result.append(";");

    }    

    return result;
}

void QnColdStoreMetaData::fromByteArray(const QByteArray& ba)
{
    m_hash.clear();

    QList<QByteArray> lst = ba.split(';');
    if (lst.size()==0)
        return;

    QString version = QString(lst.at(0));

    for (int i = 1; i < lst.size(); i+=3 )
    {
        QString fn = QString ( lst.at(i+0) );
        quint64 shift = QString ( lst.at(i+1) ).toLongLong(0, 16);
        quint64 len = QString ( lst.at(i+2) ).toLongLong(0, 16);

        m_hash.insert(fn, QnCSFileInfo(shift, len));
    }
}

bool QnColdStoreMetaData::needsToBesaved() const
{
    return m_needsToBesaved;
}

void QnColdStoreMetaData::setNeedsToBesaved(bool v)
{
    m_needsToBesaved = v;
}

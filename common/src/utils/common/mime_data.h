#ifndef QN_MIME_DATA_H
#define QN_MIME_DATA_H

#include <QtCore/QHash>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtCore/QMimeData>

class QnMimeData {
public:
    QnMimeData() {}

    explicit QnMimeData(const QMimeData *data) {
        for(const QString &format: data->formats())
            setData(format, data->data(format));
    }

    void toMimeData(QMimeData *data) const {
        for(const QString &format: data->formats())
            data->removeFormat(format);

        for(const QString &format: this->formats())
            data->setData(format, this->data(format));
    }
    
    QByteArray data(const QString &mimeType) const {
        return m_data.value(mimeType);
    }

    QStringList formats() const {
        return m_data.keys();
    }

    bool hasFormat(const QString &mimeType) const {
        return m_data.contains(mimeType);
    }

    void removeFormat(const QString &mimeType) {
        m_data.remove(mimeType);
    }

    void setData(const QString &mimeType, const QByteArray &data) {
        m_data[mimeType] = data;
    }

    friend QDataStream &operator<<(QDataStream &stream, const QnMimeData &data) {
        return stream << data.m_data;
    }

    friend QDataStream &operator>>(QDataStream &stream, QnMimeData &data) {
        return stream >> data.m_data;
    }

private:
    QHash<QString, QByteArray> m_data;
};


#endif // QN_MIME_DATA_H

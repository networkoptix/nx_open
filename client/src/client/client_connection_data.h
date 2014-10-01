#ifndef QN_CLIENT_CONNECTION_DATA_H
#define QN_CLIENT_CONNECTION_DATA_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QUrl>

// TODO: #Elric rename header to match class names

struct QnConnectionData {
    QnConnectionData(): readOnly(false) {}
    QnConnectionData(const QString &name, const QUrl &url, bool readOnly = false): name(name), url(url), readOnly(readOnly) {}

    bool operator==(const QnConnectionData &other) const {
        return name == other.name && url == other.url;
    }

    bool operator!=(const QnConnectionData &other) const {
        return !(*this == other);
    }

    bool isValid() const {
        return url.isValid() && !url.isRelative() && !url.host().isEmpty();
    }

    QString name;
    QUrl url;
    bool readOnly;
};

/**
 * @brief The QnConnectionDataList class        List of all stored connection credentials.
 */
class QnConnectionDataList: public QList<QnConnectionData> {
public:
    QnConnectionDataList(): QList<QnConnectionData>(){}

    /**
     * @brief contains                          Check if list contains a connection with the name provided.
     * @param name                              Name of the connection.
     * @return                                  True if the list contains a connection with the provided name, false otherwise.
     */
    bool contains(const QString &name);

    /**
     * @brief getByName                         Find connection info by its name.
     * @param name                              Name of the connection.
     * @return                                  First occurrence of a connection in the list with the
     *                                          name provided. Returns empty connection if no such connection was found.
     */
    QnConnectionData getByName(const QString &name);

    /**
     * @brief removeOne                         Remove from the list the first occurrence of a connection with the name provided.
     * @param name                              Name of the connection.
     * @return                                  True if any connection was removed, false otherwise.
     */
    bool removeOne(const QString &name);

    /**
     * @brief generateUniqueName                Generate the unique name with the provided name as the base,
     *                                          appending number to the end of the string.
     * @param base                              Base name of the connection.
     * @return                                  Unique name, that is not exists in the list.
     */
    QString generateUniqueName(const QString &base);

    /**
     * @brief defaultLastUsedNameKey            Get the name for the default connection as it stored in settings.
     * @return                                  Constant "Last used connection" string.
     */
    static QString defaultLastUsedNameKey();
};

Q_DECLARE_METATYPE(QnConnectionData)
Q_DECLARE_METATYPE(QnConnectionDataList)

#endif // CLIENT_CONNECTION_DATA_H

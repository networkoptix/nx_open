
#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QUrl>

struct QnConnectionData
{
    QnConnectionData();
    QnConnectionData(const QString &name, const QUrl &url,
        const QnUuid& localId);

    bool isValid() const;
    bool isCustom() const;

    bool operator==(const QnConnectionData &other) const;
    bool operator!=(const QnConnectionData &other) const;

    QString name;
    QUrl url;
    QnUuid localId;
};

Q_DECLARE_METATYPE(QnConnectionData)

/**
 * @brief The QnConnectionDataList class        List of all stored connection credentials.
 */
class QnConnectionDataList: public QList<QnConnectionData>
{
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

    int getIndexByName(const QString& name) const;
};

Q_DECLARE_METATYPE(QnConnectionDataList)

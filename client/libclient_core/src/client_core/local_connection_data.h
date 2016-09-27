
#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>

class QSettings;

struct QnLocalConnectionData
{
    QnLocalConnectionData();

    QnLocalConnectionData(const QString& name,
        const QString& systemName,
        const QString& systemId,
        const QUrl& url,
        bool isStoredPassword);

    QString name;           //< Alias of connection. Used for saved connections in dialogs.
                            // Empty alias means that it was real (not saved) connection.
    QString systemName;
    QString systemId;

    QUrl url;
    bool isStoredPassword;

    qreal weight;
    qint64 lastConnectedUtcMs;

    static void writeToSettings(QSettings* settings
        , QnLocalConnectionData data);

    static QnLocalConnectionData fromSettings(QSettings* settings);
};

#define QnLocalConnectionData_Fields (name)(systemName)(systemId)(url)(isStoredPassword)\
    (weight)(lastConnectedUtcMs)

QN_FUSION_DECLARE_FUNCTIONS(QnLocalConnectionData, (datastream)(metatype)(eq))


struct QnLocalConnectionDataList : public QList<QnLocalConnectionData>
{
    using base_type = QList<QnLocalConnectionData>;

public:
    QnLocalConnectionDataList();

    ~QnLocalConnectionDataList();

    int getIndexByName(const QString& name) const;

    QnLocalConnectionData getByName(const QString& name) const;

    using base_type::contains;
    bool contains(const QString& name) const;

    QString generateUniqueName(const QString &base) const;

    bool remove(const QString &name);
};

Q_DECLARE_METATYPE(QnLocalConnectionDataList)

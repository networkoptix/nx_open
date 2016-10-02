
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

    static QnLocalConnectionData fromSettings(QSettings* settings);

    void writeToSettings(QSettings* settings) const;
};

#define QnLocalConnectionData_Fields (name)(systemName)(systemId)(url)(isStoredPassword)
QN_FUSION_DECLARE_FUNCTIONS(QnLocalConnectionData, (datastream)(metatype)(eq))

struct QnLocalConnectionWeightData
{
    QString systemId;
    qreal weight;
    qint64 lastConnectedUtcMs;

    static QnLocalConnectionWeightData fromSettings(QSettings* settings);

    void writeToSettings(QSettings* settings) const;
};
typedef QList<QnLocalConnectionWeightData> QnLocalConnectionWeightDataList;

#define QnLocalConnectionWeightData_Fields (systemId)(weight)(lastConnectedUtcMs)
QN_FUSION_DECLARE_FUNCTIONS(QnLocalConnectionWeightData, (datastream)(metatype)(eq))
Q_DECLARE_METATYPE(QnLocalConnectionWeightDataList)

struct QnLocalConnectionDataList : public QList<QnLocalConnectionData>
{
    using base_type = QList<QnLocalConnectionData>;

public:
    QnLocalConnectionDataList();

    QnLocalConnectionDataList(const base_type& data);

    ~QnLocalConnectionDataList();

    int getIndexByName(const QString& name) const;

    QnLocalConnectionData getByName(const QString& name) const;

    using base_type::contains;
    bool contains(const QString& name) const;

    QString generateUniqueName(const QString &base) const;

    bool remove(const QString &name);
};

Q_DECLARE_METATYPE(QnLocalConnectionDataList)

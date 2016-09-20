
#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>

class QSettings;

struct QnUserRecentConnectionData
{
    QnUserRecentConnectionData();

    QnUserRecentConnectionData(const QString& name,
        const QString& systemName,
        const QUrl& url,
        bool isStoredPassword);

    QString name;           //< Alias of connection. Used for saved connections in dialogs.
                            // Empty alias means that it was real (not saved) connection.
    QString systemName;
    QUrl url;
    bool isStoredPassword;

    static void writeToSettings(QSettings* settings
        , const QnUserRecentConnectionData& data);

    static QnUserRecentConnectionData fromSettings(QSettings* settings);
};

#define QnUserRecentConnectionData_Fields (systemName)(url)(isStoredPassword)
QN_FUSION_DECLARE_FUNCTIONS(QnUserRecentConnectionData, (datastream)(metatype)(eq))


struct QnUserRecentConnectionDataList : public QList<QnUserRecentConnectionData>
{
    using base_type = QList<QnUserRecentConnectionData>;

public:
    QnUserRecentConnectionDataList();

    ~QnUserRecentConnectionDataList();

    int getIndexByName(const QString& name) const;

    QnUserRecentConnectionData getByName(const QString& name) const;

    using base_type::contains;
    bool contains(const QString& name) const;

    QString generateUniqueName(const QString &base) const;

    bool remove(const QString &name);

    void updateStorePasswordState(const QString &name, bool isPassword);
};

Q_DECLARE_METATYPE(QnUserRecentConnectionDataList)

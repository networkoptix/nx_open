// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <ui/models/sort_filter_list_model.h>
#include <nx/vms/api/data/software_version.h>

class QnSystemHostsModel: public QnSortFilterListModel
{
    Q_OBJECT

    Q_PROPERTY(QString systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)
    Q_PROPERTY(QUuid localSystemId READ localSystemId WRITE setLocalSystemId
        NOTIFY localSystemIdChanged)
    Q_PROPERTY(bool forcedLocalhostConversion READ forcedLocalhostConversion
        WRITE setForcedLocalhostConversion)

    using base_type = QnSortFilterListModel;

public:
    enum Roles
    {
        UrlRole = Qt::UserRole + 1,
        UrlDisplayRole,
        ServerIdRole,
    };

    QnSystemHostsModel(QObject* parent = nullptr);

    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    static bool isLocalhost(const QString& host, bool forcedConversion);

    Q_INVOKABLE QString getRequiredSystemVersion(int hostIndex) const;

public: // properties
    QString systemId() const;
    void setSystemId(const QString& id);

    QUuid localSystemId() const;
    void setLocalSystemId(const QUuid& id);

    bool forcedLocalhostConversion() const;
    void setForcedLocalhostConversion(bool convert);

signals:
    void systemIdChanged();
    void localSystemIdChanged();

private:
    class HostsModel;
    HostsModel* hostsModel() const;
};

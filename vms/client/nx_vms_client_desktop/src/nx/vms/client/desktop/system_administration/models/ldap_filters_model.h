// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/user_group_data.h>

namespace nx::vms::client::desktop {

struct LdapFilter
{
    Q_GADGET

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString base MEMBER base)
    Q_PROPERTY(QString filter MEMBER filter)

public:
    bool operator==(const LdapFilter&) const = default;

    QString name;
    QString base;
    QString filter;
};

struct LdapSettings
{
    Q_GADGET

    Q_PROPERTY(bool isValid MEMBER isValid)
    Q_PROPERTY(QString uri MEMBER uri)
    Q_PROPERTY(QString adminDn MEMBER adminDn)
    Q_PROPERTY(QString password MEMBER password)
    Q_PROPERTY(bool startTls MEMBER startTls)
    Q_PROPERTY(bool ignoreCertErrors MEMBER ignoreCertErrors)
    Q_PROPERTY(QList<LdapFilter> filters MEMBER filters)
    Q_PROPERTY(bool continuousSyncEnabled MEMBER continuousSyncEnabled)
    Q_PROPERTY(int continuousSync MEMBER continuousSync)
    Q_PROPERTY(int syncIntervalS MEMBER syncIntervalS)
    Q_PROPERTY(int searchTimeoutS MEMBER searchTimeoutS)

    Q_PROPERTY(QString loginAttribute MEMBER loginAttribute)
    Q_PROPERTY(QString groupObjectClass MEMBER groupObjectClass)
    Q_PROPERTY(QString memberAttribute MEMBER memberAttribute)

    Q_PROPERTY(QnUuid preferredSyncServer MEMBER preferredSyncServer)
    Q_PROPERTY(bool isHttpDigestEnabledOnImport MEMBER isHttpDigestEnabledOnImport)
    Q_PROPERTY(bool isHttpDigestEnabledOnImportInitial MEMBER isHttpDigestEnabledOnImportInitial)

public:
    bool isValid = false;
    QString uri;
    QString adminDn;
    QString password;
    bool startTls = false;
    bool ignoreCertErrors = false;
    QList<LdapFilter> filters;
    bool continuousSyncEnabled = true;
    int continuousSync = (int) nx::vms::api::LdapSettings::Sync::usersAndGroups;
    int syncIntervalS = 3600; //< 1 hour.
    int searchTimeoutS = 60; //< 1 minute.

    QString loginAttribute;
    QString groupObjectClass;
    QString memberAttribute;

    QnUuid preferredSyncServer;
    bool isHttpDigestEnabledOnImport = false;
    bool isHttpDigestEnabledOnImportInitial = false;
};

class LdapFiltersModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(QList<LdapFilter> filters READ filters WRITE setFilters NOTIFY filtersChanged)

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        BaseRole,
        FilterRole,
    };

public:
    explicit LdapFiltersModel(QObject* parent = nullptr);
    virtual ~LdapFiltersModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;

    static void registerQmlType();

    Q_INVOKABLE void addFilter(const QString& name, const QString& base, const QString& filter);
    Q_INVOKABLE void removeFilter(int row);
    Q_INVOKABLE void setFilter(
        int row, const QString& name, const QString& base, const QString& filter);

    QList<LdapFilter> filters() const;
    void setFilters(const QList<LdapFilter>& value);

    QHash<int, QByteArray> roleNames() const override;

signals:
    void filtersChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

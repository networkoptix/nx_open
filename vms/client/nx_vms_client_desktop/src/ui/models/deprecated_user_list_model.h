// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnDeprecatedUserListModelPrivate;
class QnDeprecatedUserListModel : public QAbstractListModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    enum Columns {
        CheckBoxColumn,
        UserTypeColumn,
        LoginColumn,
        FullNameColumn,
        UserRoleColumn,
        EnabledColumn,

        ColumnCount
    };

    QnDeprecatedUserListModel(QObject* parent = nullptr);
    virtual ~QnDeprecatedUserListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr& user = QnUserResourcePtr());

    bool isUserEnabled(const QnUserResourcePtr& user) const;
    void setUserEnabled(const QnUserResourcePtr& user, bool enabled);
    bool isDigestEnabled(const QnUserResourcePtr& user) const;
    void setDigestEnabled(const QnUserResourcePtr& user, bool enabled);

    QnUserResourceList users() const;
    void resetUsers(const QnUserResourceList& value);
    void addUser(const QnUserResourcePtr& user);
    void removeUser(const QnUserResourcePtr& user);

    static bool isInteractiveColumn(int column);

private:
    QnDeprecatedUserListModelPrivate* d;
    friend class QnDeprecatedUserListModelPrivate;
};

class QnDeprecatedSortedUserListModel : public QSortFilterProxyModel
{
    typedef QSortFilterProxyModel base_type;

public:
    QnDeprecatedSortedUserListModel(QObject* parent);
    void setDigestFilter(std::optional<bool> value);

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    std::optional<bool> m_digestFilter;
};

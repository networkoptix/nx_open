// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>

class QAbstractListModel;

class QnSortFilterListModelPrivate;
class NX_VMS_CLIENT_CORE_API QnSortFilterListModel: public QAbstractProxyModel
{
    Q_OBJECT
    using base_type = QAbstractProxyModel;

    Q_PROPERTY(int acceptedRowCount READ acceptedRowCount NOTIFY acceptedRowCountChanged)
    Q_PROPERTY(int filterRole READ filterRole WRITE setFilterRole NOTIFY filterRoleChanged)
    Q_PROPERTY(int filterCaseSensitivity READ filterCaseSensitivity
        WRITE setFilterCaseSensitivity NOTIFY filterCaseSensitivityChanged)
    Q_PROPERTY(QString filterWildcard READ filterWildcard
        WRITE setFilterWildcard NOTIFY filterWildcardChanged)

public:
    QnSortFilterListModel(QObject* parent = nullptr);

    ~QnSortFilterListModel();

public:
    using RolesSet = QSet<int>;
    void setTriggeringRoles(const RolesSet& roles);

    virtual void forceUpdate();

public: //< Properties
    /** Number of rows accepted by `filterAcceptsRow` only, wildcard accepts are not counted. */
    int acceptedRowCount() const;

    int filterRole() const;
    void setFilterRole(int value);

    int filterCaseSensitivity() const;
    void setFilterCaseSensitivity(int value);

    QString filterWildcard() const;
    void setFilterWildcard(const QString& value);

public:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const;

public: // overrides section
    virtual void setSourceModel(QAbstractItemModel* model) override;

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& child) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

signals:
    void acceptedRowCountChanged();
    void filterRoleChanged();
    void filterCaseSensitivityChanged();
    void filterWildcardChanged();

private:
    Q_DECLARE_PRIVATE(QnSortFilterListModel)
    const QScopedPointer<QnSortFilterListModelPrivate> d_ptr;
};

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <common/common_globals.h>
#include <client_core/connection_context_aware.h>
#include <nx/utils/uuid.h>

class QnCameraListModelPrivate;

class QnCameraListModel: public QSortFilterProxyModel, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QVariant filterIds READ filterIds WRITE setFilterIds
        NOTIFY filterIdsChanged)
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = QSortFilterProxyModel;

public:
    QnCameraListModel(QObject* parent = nullptr);
    virtual ~QnCameraListModel() override;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QVariant filterIds() const;
    void setFilterIds(const QVariant& ids);

    int count() const;

    Q_INVOKABLE int rowByResourceId(const QString& resourceId) const;
    Q_INVOKABLE QString resourceIdByRow(int row) const;

    Q_INVOKABLE QString nextResourceId(const QString& resourceId) const;
    Q_INVOKABLE QString previousResourceId(const QString& resourceId) const;

public slots:
    void refreshThumbnail(int row);
    void refreshThumbnails(int from, int to);

signals:
    void layoutIdChanged();
    void countChanged();
    void filterIdsChanged();

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QScopedPointer<QnCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCameraListModel)
};

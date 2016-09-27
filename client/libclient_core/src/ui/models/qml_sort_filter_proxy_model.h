#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <watchers/cloud_status_watcher.h>

class QnQmlSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

public:
    QnQmlSortFilterProxyModel(QObject* parent = nullptr);

    virtual ~QnQmlSortFilterProxyModel() = default;

protected:
    virtual bool lessThan(const QModelIndex& left,
        const QModelIndex& right) const override;

private:
    void handleCloudSystemsChanged(const QnCloudSystemList& systems);

    void handleLocalConnectionsChanged();

private:
    typedef QHash<QString, qreal> IdWeightHash;

    IdWeightHash m_cloudSystemWeight;
    IdWeightHash m_localSystemWeight;
};
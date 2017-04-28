#pragma once

#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <ui/models/sort_filter_list_model.h>

class QnSystemHostsModel: public QnSortFilterListModel
{
    Q_OBJECT

    Q_PROPERTY(QString systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)
    Q_PROPERTY(QUuid localSystemId READ localSystemId WRITE setLocalSystemId
        NOTIFY localSystemIdChanged)

    using base_type = QnSortFilterListModel;

public:
    enum Roles
    {
        UrlRole = Qt::UserRole + 1
    };

    QnSystemHostsModel(QObject* parent = nullptr);

    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

public: // properties
    QString systemId() const;
    void setSystemId(const QString& id);

    QUuid localSystemId() const;
    void setLocalSystemId(const QUuid& id);

signals:
    void systemIdChanged();
    void localSystemIdChanged();

private:
    class HostsModel;
    HostsModel* hostsModel() const;
};

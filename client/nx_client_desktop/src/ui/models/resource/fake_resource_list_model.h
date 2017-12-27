#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/uuid.h>

struct QnFakeResourceDescription
{
    QString id;
    QIcon icon;
    QString name;
    QString address;
};

using QnFakeResourceList = QList<QnFakeResourceDescription>;

class QnFakeResourceListModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractItemModel;

public:
    enum Columns
    {
        iconColumn,
        nameColumn,
        addressColumn,

        count
    };

    explicit QnFakeResourceListModel(
        const QnFakeResourceList& fakeResources,
        QObject *parent = nullptr);

    virtual ~QnFakeResourceListModel();

    QnFakeResourceList resources() const;

public:
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

private:
    QnFakeResourceList m_resources;
};

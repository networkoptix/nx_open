#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

struct FakeResourceDescription
{
    QString id;
    QIcon icon;
    QString name;
    QString address;
};

using FakeResourceList = QList<FakeResourceDescription>;

class FakeResourceListModel: public QAbstractListModel
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

    explicit FakeResourceListModel(
        const FakeResourceList& fakeResources,
        QObject* parent = nullptr);

    virtual ~FakeResourceListModel() override;

    FakeResourceList resources() const;

public:
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    FakeResourceList m_resources;
};

} // namespace desktop
} // namespace client
} // namespace nx

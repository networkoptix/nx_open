#pragma once

#include <QtCore/QUuid>
#include <QtCore/QAbstractListModel>

namespace nx {
namespace client {
namespace mobile {

class PtzPresetModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(QUuid uniqueResourceId READ uniqueResourceId
        WRITE setUniqueResourceId NOTIFY uniqueResourceIdChanged)

public:
    PtzPresetModel(QObject* parent = nullptr);
    virtual ~PtzPresetModel();

    QUuid uniqueResourceId() const;
    void setUniqueResourceId(const QUuid& value);

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void uniqueResourceIdChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx


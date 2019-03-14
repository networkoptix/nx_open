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

    Q_PROPERTY(QString resourceId READ resourceId
        WRITE setResourceId NOTIFY resourceIdChanged)

public:
    PtzPresetModel(QObject* parent = nullptr);
    virtual ~PtzPresetModel();

    QString resourceId() const;
    void setResourceId(const QString& value);

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void resourceIdChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx


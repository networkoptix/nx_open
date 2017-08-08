#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class AnalyticsSdkEventModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;
public:
    enum DataRole
    {
        EventTypeIdRole = Qt::UserRole + 1,
    };

    AnalyticsSdkEventModel(QObject* parent = nullptr);

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    void loadFromCameras(const QnVirtualCameraResourceList& cameras);

    bool isValid() const;

private:
    QMap<QnUuid, QString> m_usedDrivers;

    struct Item
    {
        QnUuid driverId;
        QnUuid eventTypeId;
        QString eventName;
    };

    QList<Item> m_items;
    bool m_valid = false;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

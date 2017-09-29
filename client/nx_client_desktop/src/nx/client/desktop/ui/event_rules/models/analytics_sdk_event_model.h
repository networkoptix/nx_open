#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

namespace nx { namespace api { struct AnalyticsEventTypeWithRef; } }

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
        DriverIdRole,
    };

    AnalyticsSdkEventModel(QObject* parent = nullptr);
    ~AnalyticsSdkEventModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    void loadFromCameras(const QnVirtualCameraResourceList& cameras);

    bool isValid() const;

private:
    struct Private;
    QScopedPointer<Private> d;

};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

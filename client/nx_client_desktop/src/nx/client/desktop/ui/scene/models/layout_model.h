#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace scene {
namespace models {

class LayoutModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

    Q_OBJECT
    Q_PROPERTY(QnUuid layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)
    Q_PROPERTY(QRect gridBoundingRect READ gridBoundingRect NOTIFY gridBoundingRectChanged)

public:
    enum class Roles
    {
        itemId = Qt::UserRole + 1,
        name,
        geometry,
    };

    explicit LayoutModel(QObject* parent = nullptr);
    virtual ~LayoutModel() override;

    QnUuid layoutId() const;
    void setLayoutId(const QnUuid& id);

    QRect gridBoundingRect() const;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

signals:
    void layoutIdChanged();
    void gridBoundingRectChanged();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace models
} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

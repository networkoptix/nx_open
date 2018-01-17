#pragma once

#include <QtCore/QScopedPointer>
#include <QtGui/QPixmap>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/common/utils/command_action.h>
#include <nx/client/desktop/event_search/models/abstract_event_list_model.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class EventListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    struct EventData
    {
        QnUuid id;
        QString title;
        QString description;
        QString toolTip;
        qint64 timestampMs = 0;
        QPixmap icon;
        QColor titleColor;
        bool removable = false;
        int lifetimeMs = -1;
        int helpId = -1;
        QnVirtualCameraResourcePtr previewCamera;
        QnVirtualCameraResourceList cameras;
        qint64 previewTimeMs = 0; //< The latest thumbnail is used if previewTimeMs <= 0.
        ui::action::IDType actionId = ui::action::NoAction;
        ui::action::Parameters actionParameters;
        CommandActionPtr extraAction;
        QVariant extraData;
    };

public:
    explicit EventListModel(QObject* parent = nullptr);
    virtual ~EventListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    virtual bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qn::DefaultNotificationRole) override;

    void clear();

protected:
    enum class Position
    {
        front,
        back
    };

    bool addEvent(const EventData& event, Position where = Position::front);
    bool updateEvent(const EventData& event);
    bool removeEvent(const QnUuid& id);

    QModelIndex indexOf(const QnUuid& id) const;
    EventData getEvent(int row) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

#pragma once

#include "../system_health_list_model.h"

#include <deque>

#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtWidgets/QAction>

#include <core/resource/resource_fwd.h>
#include <health/system_health.h>

#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

namespace nx::vms::client::desktop {

class SystemHealthListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(SystemHealthListModel* q);
    virtual ~Private() override;

    int count() const;

    QnSystemHealth::MessageType message(int index) const;
    QnResourcePtr resource(int index) const;

    QString text(int index) const;
    QString toolTip(int index) const;
    QPixmap pixmap(int index) const;
    QColor color(int index) const;
    QnResourceList displayedResourceList(int index) const;
    int helpId(int index) const;
    int priority(int index) const;
    bool locked(int index) const;
    bool isCloseable(int index) const;
    CommandActionPtr commandAction(int index) const; //< Additional button action with parameters.
    ui::action::IDType action(int index) const; //< Click-on-tile action id.
    ui::action::Parameters parameters(int index) const; // Click-on-tile action parameters.

    void remove(int first, int count);

private:
    void addItem(QnSystemHealth::MessageType message, const QVariant& params);
    void removeItem(QnSystemHealth::MessageType message, const QVariant& params);
    void toggleItem(QnSystemHealth::MessageType message, bool isOn);
    void updateItem(QnSystemHealth::MessageType message);
    void updateCachedData(QnSystemHealth::MessageType message);
    void clear();

    static int priority(QnSystemHealth::MessageType message);
    static QPixmap pixmap(QnSystemHealth::MessageType message);

private:
    struct Item
    {
        Item() = default;
        Item(QnSystemHealth::MessageType message, const QnResourcePtr& resource);

        operator QnSystemHealth::MessageType() const; //< Implicit by design.
        bool operator==(const Item& other) const;
        bool operator!=(const Item& other) const;
        bool operator<(const Item& other) const;

        QnSystemHealth::MessageType message = QnSystemHealth::MessageType::Count;
        QnResourcePtr resource;
        vms::event::AbstractActionPtr serverData;
    };

private:
    SystemHealthListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    std::deque<Item> m_items; //< Kept sorted.
    QSet<QnSystemHealth::MessageType> m_popupSystemHealthFilter;
    QnUserResourceList m_usersWithInvalidEmail;
    QnVirtualCameraResourceList m_camerasWithDefaultPassword;
};

} // namespace nx::vms::client::desktop

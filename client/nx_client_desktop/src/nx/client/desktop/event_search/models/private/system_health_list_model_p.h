#pragma once

#include "../system_health_list_model.h"

#include <deque>

#include <core/resource/resource_fwd.h>
#include <health/system_health.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

namespace nx {
namespace client {
namespace desktop {

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
    int helpId(int index) const;
    int priority(int index) const;
    bool locked(int index) const;

    ui::action::IDType action(int index) const;
    ui::action::Parameters parameters(int index) const;

    void remove(int first, int count);

private:
    void addSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);
    void removeSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);
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
    };

private:
    SystemHealthListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    std::deque<Item> m_items; //< Kept sorted.
};

} // namespace
} // namespace client
} // namespace nx

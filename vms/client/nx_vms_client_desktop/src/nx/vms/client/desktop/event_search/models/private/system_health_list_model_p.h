// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtGui/QAction>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/menu/action_fwd.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/common/system_health/message_type.h>

#include "../system_health_list_model.h"

namespace nx::vms::client::desktop {

class SystemHealthListModel::Private:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    using MessageType = common::system_health::MessageType;

    explicit Private(SystemHealthListModel* q);
    virtual ~Private() override;

    int count() const;

    MessageType message(int index) const;
    QnResourcePtr resource(int index) const;

    QString text(int index) const;
    QString toolTip(int index) const;
    QString decorationPath(int index) const;
    QColor color(int index) const;
    QVariant timestamp(int index) const;
    QnResourceList displayedResourceList(int index) const;
    int helpId(int index) const;
    int priority(int index) const;
    bool locked(int index) const;
    bool isCloseable(int index) const;
    CommandActionPtr commandAction(int index) const; //< Additional button action with parameters.
    menu::IDType action(int index) const; //< Click-on-tile action id.
    menu::Parameters parameters(int index) const; // Click-on-tile action parameters.
    MessageType messageType(int index) const;

    void remove(int first, int count);

private:
    void doAddItem(MessageType message, const QVariant& params, bool initial);
    void addItem(MessageType message, const QVariant& params);
    void removeItem(MessageType message, const QVariant& params);
    void removeItemForResource(MessageType message, const QnResourcePtr& resource);
    void toggleItem(MessageType message, bool isOn);
    void updateItem(MessageType message);
    void clear();

    QSet<QnResourcePtr> getResourceSet(MessageType message) const;
    QnResourceList getSortedResourceList(MessageType message) const;

    static int priority(MessageType message);
    static QString decorationPath(MessageType message);

private:
    struct Item
    {
        Item() = default;
        Item(MessageType message, const QnResourcePtr& resource);

        operator MessageType() const { return message; } //< Implicit by design.
        bool operator==(const Item& other) const;
        bool operator!=(const Item& other) const;
        bool operator<(const Item& other) const;

        MessageType message = MessageType::count;
        QnResourcePtr resource;
        vms::event::AbstractActionPtr serverData;
    };

private:
    SystemHealthListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    std::deque<Item> m_items; //< Kept sorted.
    std::set<MessageType> m_popupSystemHealthFilter;
};

} // namespace nx::vms::client::desktop

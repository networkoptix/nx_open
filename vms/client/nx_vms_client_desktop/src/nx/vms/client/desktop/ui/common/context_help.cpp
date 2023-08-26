// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context_help.h"

#include <QtQml/QtQml>

#include <nx/vms/client/desktop/help/help_topic_accessor.h>

namespace nx::vms::client::desktop {

ContextHelpAttached::ContextHelpAttached(QObject* parent): QObject(parent)
{
}

int ContextHelpAttached::topicId() const
{
    return HelpTopicAccessor::helpTopic(parent());
}

void ContextHelpAttached::setTopicId(int value)
{
    if (topicId() == value)
        return;

    HelpTopicAccessor::setHelpTopic(parent(), value);
    emit topicIdChanged();
}

ContextHelpAttached* ContextHelp::qmlAttachedProperties(QObject* parent)
{
    return new ContextHelpAttached(parent);
}

void ContextHelp::registerQmlType()
{
    qmlRegisterType<ContextHelp>("nx.vms.client.desktop", 1, 0, "ContextHelp");
}

} // namespace nx::vms::client::desktop

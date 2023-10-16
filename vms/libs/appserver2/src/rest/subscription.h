// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace ec2 {

enum class NotifyType
{
    update,
    delete_
};

using SubscriptionCallback = nx::utils::MoveOnlyFunc<void(const QString& id, NotifyType)>;

} // namespace ec2

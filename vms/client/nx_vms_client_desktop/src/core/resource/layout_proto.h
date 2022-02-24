// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QByteArray;

class QnProtoValue;

namespace nx::vms::api { struct LayoutData; }

namespace QnProto {

    template<class T>
    struct Message {
        T data;
    };

    bool deserialize(const QByteArray &value, Message<nx::vms::api::LayoutData> *target);
    bool deserialize(const QnProtoValue &value, Message<nx::vms::api::LayoutData> *target);
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

RadioGroup
{
    id: control

    property Analytics.Attribute attribute
    model: CoreUtils.toArray(attribute && attribute.enumeration && attribute.enumeration.items)
}

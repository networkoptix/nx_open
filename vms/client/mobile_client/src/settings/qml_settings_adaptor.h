// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQml/QQmlPropertyMap>
#include <QtCore/QVariant>

namespace nx::client::mobile {

class QmlSettingsAdaptor: public QQmlPropertyMap
{
public:
    explicit QmlSettingsAdaptor(QObject* parent = nullptr);
};

} // namespace nx::client::mobile

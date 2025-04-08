// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::mobile {

class WindowContext;

namespace detail {

class Screens: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    Screens(WindowContext* context, QObject* parent = nullptr);

    /** Show 2FA validation screen and validate specified token. */
    bool show2faValidationScreen(const network::http::Credentials& credentials) const;
};

} // namespace detail

} // namespace nx::vms::client::mobile

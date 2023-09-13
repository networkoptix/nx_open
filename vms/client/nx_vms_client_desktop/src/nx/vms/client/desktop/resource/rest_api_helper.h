// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::desktop {

class FreshSessionTokenHelper;

class RestApiHelper: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    explicit RestApiHelper(QObject* parent = nullptr);
    virtual ~RestApiHelper() override;

    void setParent(QWidget* parentWidget);
    nx::vms::common::SessionTokenHelperPtr getSessionTokenHelper();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

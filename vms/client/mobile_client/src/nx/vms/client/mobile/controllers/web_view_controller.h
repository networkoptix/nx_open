// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile {

class WebViewController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    WebViewController(QObject* parent = nullptr);
    ~WebViewController();

    void openUrl(const QUrl& url);

    void closeWindow();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_http_headers_dialog.h"

#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop::rules {

CustomHttpHeadersDialog::CustomHttpHeadersDialog(
    QList<vms::rules::KeyValueObject> headers,
    QWidget* parent)
    :
    QnSessionAwareDelegate(parent),
    m_headersModel{new KeyValueModel{std::move(headers), this}}
{
    setTransientParent(parent->window()->windowHandle());
    setInitialProperties({{"headersModel", QVariant::fromValue(m_headersModel)}});
    setSource(QUrl("Nx/VmsRules/CustomHttpHeadersDialog.qml"));
}

const QList<vms::rules::KeyValueObject>& CustomHttpHeadersDialog::headers() const
{
    return m_headersModel->data();
}

bool CustomHttpHeadersDialog::tryClose(bool /*force*/)
{
    reject();

    return true;
}

} // namespace nx::vms::client::desktop::rules

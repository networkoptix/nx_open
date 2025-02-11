// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_http_headers_dialog.h"

#include <nx/vms/client/desktop/application_context.h>

#include "../../model_view/key_value_model.h"

namespace nx::vms::client::desktop::rules {

CustomHttpHeadersDialog::CustomHttpHeadersDialog(
    QList<vms::rules::KeyValueObject>& headers,
    QWidget* parent)
    :
    QnSessionAwareDelegate(parent)
{
    setTransientParent(parent->window()->windowHandle());
    setInitialProperties({{"headersModel", QVariant::fromValue(new KeyValueModel{headers, this})}});
    setSource(QUrl("Nx/VmsRules/CustomHttpHeadersDialog.qml"));
}

bool CustomHttpHeadersDialog::tryClose(bool /*force*/)
{
    reject();

    return true;
}

} // namespace nx::vms::client::desktop::rules

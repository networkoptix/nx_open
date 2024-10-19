// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../../token_data_provider.h"

#include <QtCore/QJniObject>

namespace {

static const auto kBaiduHelperClass = "com/nxvms/mobile/push/baidu/BaiduHelper";

} // namespace

namespace nx::vms::client::mobile::details {

TokenProviderType TokenDataProvider::type() const
{
    return TokenProviderType::baidu;
}

bool TokenDataProvider::requestTokenDataUpdate()
{
    QJniObject::callStaticMethod<void>(kBaiduHelperClass, "requestToken");
    return true;
}

bool TokenDataProvider::requestTokenDataReset()
{
    QJniObject::callStaticMethod<void>(kBaiduHelperClass, "resetToken");
    return true;
}

} // namespace nx::vms::client::mobile::details

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_show_once_settings.h"

namespace {

static const QString kClientShowOnceId("show_once");

} // namespace

template<> QnClientShowOnceSettings* Singleton<QnClientShowOnceSettings>::s_instance = nullptr;

QnClientShowOnceSettings::QnClientShowOnceSettings(QObject* parent):
    base_type(kClientShowOnceId, base_type::StorageFormat::Section, parent)
{
}

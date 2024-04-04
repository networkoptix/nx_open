// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

class SystemContext;

class SelectAnalyticsObjectTypesButton: public QPushButton
{
    Q_OBJECT

public:
    SelectAnalyticsObjectTypesButton(QWidget* parent = nullptr);

    void setSelectedObjectTypes(SystemContext* context, const QStringList& ids);
    void setSelectedAllObjectTypes();
};

} // namespace nx::vms::client::desktop

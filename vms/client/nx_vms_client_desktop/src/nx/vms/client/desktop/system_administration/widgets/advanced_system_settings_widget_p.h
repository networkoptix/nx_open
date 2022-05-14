// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "advanced_system_settings_widget.h"

#include <QtCore/QStringList>

class QQuickWidget;
class QStackedWidget;

namespace nx::vms::client::desktop {

class AdvancedSystemSettingsWidget::Private: public QObject
{
    Q_OBJECT

public:
    Private(AdvancedSystemSettingsWidget* q);

    void addTab(const QString& name, QWidget* widget);

    Q_INVOKABLE void setCurrentTab(int idx);

private:
    AdvancedSystemSettingsWidget* const q = nullptr;

    QStringList m_items;
    QQuickWidget* m_menu = nullptr;
    QStackedWidget* m_stack = nullptr;
};

} // namespace nx::vms::client::desktop

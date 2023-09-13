// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include "advanced_system_settings_widget.h"

class QQuickWidget;
class QStackedWidget;

namespace nx::vms::client::desktop {

class AdvancedSystemSettingsWidget::Private: public QObject
{
    Q_OBJECT

public:
    static QUrl urlFor(AdvancedSystemSettingsWidget::Subpage page);

    Private(AdvancedSystemSettingsWidget* q);

    void addTab(const QString& name, const QUrl& url, QWidget* widget);
    bool openSubpage(const QUrl& url);

    // Called from QML when user clicks on menu item.
    Q_INVOKABLE void setCurrentTab(int idx);

    bool backupAndRestoreIsVisible() const;
    void updateBackupAndRestoreTabVisibility();
    QList<QnAbstractPreferencesWidget*> tabs() const;

private:
    AdvancedSystemSettingsWidget* const q = nullptr;

    QStringList m_items;
    QHash<QUrl, int> m_urls;
    QQuickWidget* m_menu = nullptr;
    QStackedWidget* m_stack = nullptr;
};

} // namespace nx::vms::client::desktop

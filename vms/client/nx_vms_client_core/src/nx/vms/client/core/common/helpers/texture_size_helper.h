// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QObject>
#include <QtQuick/QQuickItem>

class QQuickWindow;

/**
 * Tracks window RHI to provide maximum texture size via maxTextureSize property.
 * When no RHI is present the maxTextureSize is set to a predefined value 2048.
 */
namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API TextureSizeHelper: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int maxTextureSize READ maxTextureSize NOTIFY maxTextureSizeChanged)

public:
    TextureSizeHelper(QQuickItem* parent = nullptr);

    int maxTextureSize() const;

    static void registerQmlType();

private slots:
    void setWindow(QQuickWindow* win);

private:
    void setMaxTextureSize(int size);

signals:
    void maxTextureSizeChanged();

private:
    QPointer<QQuickWindow> m_window;
    int m_maxTextureSize = 0;
};

} // namespace nx::vms::client::core

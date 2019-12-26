#pragma once

#include <atomic>

#include <QtCore/QObject>

class QQuickWindow;

namespace nx::vms::client::core {

class TextureSizeHelper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(int maxTextureSize READ maxTextureSize NOTIFY maxTextureSizeChanged)
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
    TextureSizeHelper(QObject* parent = nullptr);

    int maxTextureSize() const;

    QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);

    static void registerQmlType();

private:
    void setMaxTextureSize(int size);

signals:
    void maxTextureSizeChanged();
    void windowChanged();

private:
    QQuickWindow* m_window = nullptr;
    std::atomic_int m_maxTextureSize;
};

} // namespace nx::vms::client::core

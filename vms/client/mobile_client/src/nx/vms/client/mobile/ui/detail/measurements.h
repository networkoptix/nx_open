// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMargins>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QQuickWindow;

namespace nx::vms::client::mobile {

class WindowContext;

namespace detail {

class Measurements: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QMargins customMargins
        READ customMargins
        NOTIFY customMarginsChanged)

    Q_PROPERTY(int deviceStatusBarHeight
        READ deviceStatusBarHeight
        NOTIFY deviceStatusBarHeightChanged)

    /**
     * Workaround for the QTBUG-72472 - view is not changing size if there is Android WebView on the
     * scene. Also keyboard height is always 0 in this situation in Qt.
     * Check if the bug is fixed in Qt 6.x.
     */
    Q_PROPERTY(int androidKeyboardHeight
        READ androidKeyboardHeight
        NOTIFY androidKeyboardHeightChanged)

public:
    Measurements(QQuickWindow* window, QObject* parent = nullptr);
    virtual ~Measurements() override;

    QMargins customMargins();
    Q_INVOKABLE void updateCustomMargins();

    int deviceStatusBarHeight() const;

    int androidKeyboardHeight() const;

    Q_INVOKABLE int getMaxTextureSize() const;

signals:
    void customMarginsChanged();
    void deviceStatusBarHeightChanged();
    void androidKeyboardHeightChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace detail

} // namespace nx::vms::client::mobile

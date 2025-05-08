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

    /**
     * Workaround for the QTBUG-72472 - view is not changing size if there is Android WebView on the
     * scene. Also keyboard height is always 0 in this situation in Qt.
     * Check if the bug is fixed in Qt 6.x.
     */
    Q_PROPERTY(int androidKeyboardHeight
        READ androidKeyboardHeight
        NOTIFY androidKeyboardHeightChanged)

    Q_PROPERTY(int deviceStatusBarHeight
        READ deviceStatusBarHeight
        NOTIFY deviceStatusBarHeightChanged)

public:
    Measurements(QQuickWindow* window, QObject* parent = nullptr);
    virtual ~Measurements() override;

    int androidKeyboardHeight() const;

    int deviceStatusBarHeight() const;

    Q_INVOKABLE int getMaxTextureSize() const;

signals:
    void androidKeyboardHeightChanged();
    void deviceStatusBarHeightChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace detail

} // namespace nx::vms::client::mobile

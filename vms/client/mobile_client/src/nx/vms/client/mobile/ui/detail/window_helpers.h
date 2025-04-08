// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::vms::client::mobile {

class WindowContext;

namespace detail {

class WindowHelpers: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_class = QObject;

public:
    WindowHelpers(WindowContext* context,
        QObject* parent = nullptr);
    virtual ~WindowHelpers() override;

    Q_INVOKABLE int navigationBarType() const;

    Q_INVOKABLE int navigationBarSize() const;

    Q_INVOKABLE void enterFullscreen() const;

    Q_INVOKABLE void exitFullscreen() const;

    Q_INVOKABLE void copyToClipboard(const QString &text);

    Q_INVOKABLE bool isRunOnPhone() const;

    Q_INVOKABLE void setKeepScreenOn(bool keepScreenOn) const;

    Q_INVOKABLE void setScreenOrientation(Qt::ScreenOrientation orientation) const;

    Q_INVOKABLE void setGestureExclusionArea(int y, int height) const;

    Q_INVOKABLE void makeShortVibration() const;

    Q_INVOKABLE void requestRecordAudioPermissionIfNeeded() const;
};

} // namespace detail

} // namespace nx::vms::client::mobile::detail

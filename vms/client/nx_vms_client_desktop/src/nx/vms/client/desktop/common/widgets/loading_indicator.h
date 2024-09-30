// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QPixmap>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/** Generates and animates a loading indicator. */
class LoadingIndicator: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LoadingIndicator(QObject* parent = nullptr);
    virtual ~LoadingIndicator() override;

    /** Is the animation started. The value set to true after the object creation. */
    bool started() const;

    /** Starts the animation. */
    void start();

    /** Stops the animation. */
    void stop();

    /** Returns the current pixmap. */
    QPixmap currentPixmap() const;

    /**
     * Creates a pixmap for the given progess value.
     * The progress should be a value between 0.0 and 1.0
     */
    static QPixmap createPixmap(double progress = 0.0);

signals:
    /**
     * The signal is emitted for every animation frame.
     * The current frame's pixmap is passed as a parameter.
     */
    void frameChanged(const QPixmap& pixmap);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

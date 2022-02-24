// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

namespace nx::vms::client::desktop::testkit {

class FullScreenOverlay;

/** A helper class for highlighting widgets/items under cursor. */
class Highlighter: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    Highlighter(QObject* parent = nullptr);
    ~Highlighter();

    /** Get the smallest element at specified screen coordinates. */
    QVariant pick(QPoint globalPos) const;

    bool isEnabled() const;

    void setEnabled(bool enabled);

private:
    void highlight(QVariant data, std::chrono::milliseconds hideTimeout);

    QVariant objectUnder(QPoint globalPos);

    std::unique_ptr<FullScreenOverlay> m_overlay;
    QTimer m_pickTimer;
    QTimer m_hideTimer;
};

} // namespace nx::vms::client::desktop::testkit

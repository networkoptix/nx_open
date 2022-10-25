// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <variant>

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {

class ProgressState
{
public:
    enum State
    {
        completed,
        failed,
        indefinite,
    };

public:
    ProgressState() = default;
    ProgressState(State state);
    ProgressState(qreal value);

    bool isCompleted() const;
    bool isFailed() const;
    bool isIndefinite() const;
    std::optional<qreal> value() const;

    bool operator==(const ProgressState& progress) const = default;

private:
    std::variant<State, qreal> m_state = 0.0;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ProgressState)

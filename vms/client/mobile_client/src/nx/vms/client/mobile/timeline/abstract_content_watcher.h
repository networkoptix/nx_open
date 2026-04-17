// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

namespace nx::vms::client::mobile {
namespace timeline {

class AbstractContentWatcher: public QObject
{
    Q_OBJECT

public:
    // Whether the content is present in the archive.
    // Return std::nullopt if the content has never been loaded yet, or is being loaded.
    std::optional<bool> hasContent() const;

    // The forced flag should be set when and only when it's already known from other sources
    // that the content exists.
    void setForced(bool value);

protected:
    void setHasContent(std::optional<bool> value);
    bool isForced() const;

signals:
    void hasContentChanged();

private:
    std::optional<bool> m_hasContent;
    bool m_forced = false;
};

} // namespace timeline
} // namespace nx::vms::client::mobile

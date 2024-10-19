// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include "token_data.h"

namespace nx::vms::client::mobile::details {

/** Gives access to token data setters/getters and watches on value changes. */
class TokenDataWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static TokenDataWatcher& instance();

    TokenData data() const;

    void setData(const TokenData& value);

    void resetData();

    bool hasData() const;

signals:
    void tokenDataChanged();

private:
    TokenDataWatcher() = default;

private:
    using OptionalTokenData = std::optional<TokenData>;
    OptionalTokenData m_tokenData;
};

} // namespace nx::vms::client::mobile::details

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QList>
#include <QString>

#include "../rules_fwd.h"
#include "event_parameter_helper.h"

class SystemContext;

namespace nx::vms::rules::utils {

enum class TextTokenType
{
    Text,
    Substitution
};

struct TextToken
{
    TextTokenType type = TextTokenType::Text;
    QString value;
    bool isValid = true;
    qsizetype start = 0;
    qsizetype length = 0;
};

using TextTokenList = QList<TextToken>;

namespace TextTokenizer
{
    TextTokenList tokenizeText(QString text);
    QString composeTextFromTokenList(
        const TextTokenList& tokens,
        common::SystemContext* systemContext,
        const AggregatedEventPtr& eventAggregator);
}

} // namespace nx::vms::rules::utils

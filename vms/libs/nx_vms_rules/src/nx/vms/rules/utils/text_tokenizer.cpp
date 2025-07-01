// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_tokenizer.h"

namespace nx::vms::rules::utils {

TextTokenList tokenizeText(QString text)
{
    TextTokenList result;

    bool inSub = false;
    int start = 0, cur = 0;

    while (cur != text.size())
    {
        if (EventParameterHelper::isStartOfEventParameter(text[cur]))
        {
            if (start != cur)
            {
                result += {
                    .type = TextTokenType::text,
                    .value = text.mid(start, cur - start),
                    .start = start,
                    .length = cur - start,
                };
            }
            start = cur;
            inSub = true;
        }

        if (EventParameterHelper::isEndOfEventParameter(text[cur]) && inSub)
        {
            if (start + 1 != cur)
            {
                TextToken desc = {
                    .type = TextTokenType::substitution,
                    .value = text.mid(start + 1, cur - start - 1),
                    .start = start};
                desc.length = desc.value.size() + 2;
                result += desc;
            }
            else
            {
                // Empty brackets.
                result += {
                    .type = TextTokenType::text,
                    .value = "{}",
                    .start = start,
                };
            }
            start = cur + 1;
            inSub = false;
        }

        ++cur;
    }

    if (start < text.size())
    {
        result += {.type = TextTokenType::text,
            .value = text.mid(start),
            .start = start,
            .length = text.size() - start};
    }

    return result;
}

QString composeTextFromTokenList(
    const TextTokenList& tokens,
    common::SystemContext* systemContext,
    const AggregatedEventPtr& event)
{
    QString result;
    for (const auto& token: tokens)
    {
        if (token.type == TextTokenType::substitution)
        {
            result += EventParameterHelper::instance()->evaluateEventParameter(
                systemContext, event, token.value);
        }
        else
        {
            result += token.value;
        }
    }
    return result;
}

} // namespace nx::vms::rules::utils

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

#include "../aggregated_event.h"

namespace nx::vms::rules {

struct TextWithFields::Private
{
    QString text;
    TextWithFields* const q;
    QList<ValueDescriptor> values;

    void parseText()
    {
        values.clear();

        bool inSub = false;
        int start = 0, cur = 0;

        while (cur != text.size())
        {
            if (utils::EventParameterHelper::isStartOfEventParameter(text[cur]))
            {
                if (start != cur)
                {
                    values += {
                        .type = FieldType::Text,
                        .value = text.mid(start, cur - start),
                        .start = start,
                        .length = cur - start,
                    };
                }
                start = cur;
                inSub = true;
            }

            if (utils::EventParameterHelper::isEndOfEventParameter(text[cur]) && inSub)
            {
                if (start + 1 != cur)
                {
                    ValueDescriptor desc = {.type = FieldType::Substitution,
                        .value = text.mid(start + 1, cur - start - 1),
                        .start = start};
                    desc.length = desc.value.size() + 2;
                    values += desc;
                }
                else
                {
                    // Empty brackets.
                    values += {
                        .type = FieldType::Text,
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
            values += {.type = FieldType::Text,
                .value = text.mid(start),
                .start = start,
                .length = text.size() - start};
        }

        emit q->parseFinished(values);
    }
};

TextWithFields::TextWithFields(common::SystemContext* context):
    common::SystemContextAware(context),
    d(new Private{.q = this})
{
}

TextWithFields::~TextWithFields()
{
}

QVariant TextWithFields::build(const AggregatedEventPtr& eventAggregator) const
{
    QString result;
    for (const auto& value: d->values)
    {
        if (value.type == FieldType::Substitution)
        {
            result += utils::EventParameterHelper::evaluateEventParameter(
                systemContext(), eventAggregator, value.value);
        }
        else
        {
            // It's just a text, add it to the result.
            result += value.value;
        }
    }

    return result;
}

QString TextWithFields::text() const
{
    return d->text;
}

void TextWithFields::parseText()
{
    d->parseText();
}

void TextWithFields::setText(const QString& text)
{
    if (d->text == text)
        return;

    d->text = text;
    emit textChanged();
    d->parseText();
}

} // namespace nx::vms::rules

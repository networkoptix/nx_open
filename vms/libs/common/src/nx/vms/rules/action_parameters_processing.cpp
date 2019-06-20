#include "action_parameters_processing.h"

#include <nx/vms/event/event_parameters.h>

namespace nx::vms::rules {

namespace {

void processKeyword(QString& value, const QString& keyword, const QString& actualValue)
{
    value.replace(keyword, actualValue);
}

/**
 * Update actual text value with applied substitutions (if any).
 */
void processTextFieldSubstitutions(QString& value, const EventParameters& data)
{
    processKeyword(value, SubstitutionKeywords::Event::source, data.resourceName);
    processKeyword(value, SubstitutionKeywords::Event::caption, data.caption);
    processKeyword(value, SubstitutionKeywords::Event::description, data.description);
}

} // namespace

const QString SubstitutionKeywords::Event::source("{event.source}");
const QString SubstitutionKeywords::Event::caption("{event.caption}");
const QString SubstitutionKeywords::Event::description("{event.description}");

ActionParameters actualActionParameters(
    ActionType actionType,
    const ActionParameters& sourceActionParameters,
    const EventParameters& eventRuntimeParameters)
{
    ActionParameters result = sourceActionParameters;

    if (actionType == ActionType::execHttpRequestAction)
    {
        processTextFieldSubstitutions(result.text, eventRuntimeParameters);
    }
    return result;
}

} // namespace nx::vms::rules

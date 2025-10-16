// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "duration_picker_widget.h"

namespace nx::vms::client::desktop::rules {

QString DurationPickerTranslationHelper::alsoInclude()
{
    return tr(
        "Also include",
        /*comment*/ "Part of the text, action duration: "
        "Also include <time> Before Event");
}

QString DurationPickerTranslationHelper::begin()
{
    return tr(
        "Begin",
        /*comment*/ "Part of the text, action duration: "
        "Begin <time> Before Event");
}

QString DurationPickerTranslationHelper::beforeEvent()
{
    return tr(
        "Before Event",
        /*comment*/ "Part of the text, action duration: "
        "Begin <time> Before Event");
}

QString DurationPickerTranslationHelper::end()
{
    return tr(
        "End",
        /*comment*/ "Part of the text, action duration: "
        "End <time> After Event");
}

QString DurationPickerTranslationHelper::afterEvent()
{
    return tr(
        "After Event",
        /*comment*/ "Part of the text, action duration: "
        "End <time> After Event");
}

} // namespace nx::vms::client::desktop::rules

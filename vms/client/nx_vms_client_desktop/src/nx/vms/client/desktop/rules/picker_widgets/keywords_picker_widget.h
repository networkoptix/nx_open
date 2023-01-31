// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/event_filter_fields/keywords_field.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/common/elided_label.h>

#include "oneline_text_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class KeywordsPicker: public OnelineTextPickerWidgetBase<vms::rules::KeywordsField>
{
    Q_OBJECT

public:
    using OnelineTextPickerWidgetBase<vms::rules::KeywordsField>::OnelineTextPickerWidgetBase;

protected:
    void onDescriptorSet() override
    {
        OnelineTextPickerWidgetBase<vms::rules::KeywordsField>::onDescriptorSet();

        m_label->addHintLine(tr(
            "Event will trigger only if there are matches in the source with any of the entered keywords."));
        m_label->addHintLine(tr("If the field is empty, event will always trigger."));
        setHelpTopic(m_label, Qn::EventsActions_Generic_Help);

        m_lineEdit->setPlaceholderText(m_fieldDescriptor->description);
    }

    void updateValue() override
    {
        const QSignalBlocker blocker{m_lineEdit};
        m_lineEdit->setText(m_field->string());
    }

    void onTextChanged(const QString& text) override
    {
        m_field->setString(text);
    }
};

} // namespace nx::vms::client::desktop::rules

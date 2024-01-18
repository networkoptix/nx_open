// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QString>

#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {

class NX_VMS_RULES_API EventParameterHelper
{
public:
    using EventParametersNames = QList<QString>;

    static QString addBrackets(const QString& text);
    static QString removeBrackets(QString text);
    static QString getSubGroupName(const QString& text);
    static QString getMainGroupName(const QString& text);
    static bool isStartOfEventParameter(const QChar& symbol);
    static bool isEndOfEventParameter(const QChar& symbol);
    static int getLatestEventParameterPos(const QStringView& text, int stopPosition);
    static bool containsSubgroups(const QString& eventParameter);

    /**
     * Collects list of visible event parameters for a specific event Type and Analytic Object
     * Type.
     */
    static EventParametersNames getVisibleEventParameters(
        const QString& eventType,
        common::SystemContext* systemContext,
        const QString& objectTypeField,
        State eventState = State::none);
    /**
     * Generates eventParameter's replacement for specific event, if it is applicable.
     */
    static QString evaluateEventParameter(common::SystemContext* context,
        const AggregatedEventPtr& eventAggregator,
        const QString& eventParameter);
};

} // namespace nx::vms::rules::utils

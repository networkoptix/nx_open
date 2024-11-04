// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QString>
#include <set>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/string/comparator.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {

class NX_VMS_RULES_API EventParameterHelper
{
public:
    using EventParametersNames = std::set<QString, nx::utils::CaseInsensitiveStringCompare>;

    static EventParameterHelper* instance();

    static QString addBrackets(const QString& text);
    static QString removeBrackets(QString text);
    static QString getSubGroupName(const QString& text);
    static QString getMainGroupName(const QString& text);
    static bool isStartOfEventParameter(const QChar& symbol);
    static bool isIncompleteEventParameter(const QString& word);
    static bool isEndOfEventParameter(const QChar& symbol);
    static int getLatestEventParameterPos(const QStringView& text, int stopPosition);
    static bool containsSubgroups(const QString& eventParameter);
    static QChar startCharOfEventParameter();
    static QChar endCharOfEventParameter();

    /**
     * Collects list of visible event parameters for a specific event Type and Analytic Object
     * Type.
     */
    EventParametersNames getVisibleEventParameters(
        const QString& eventType,
        common::SystemContext* systemContext,
        const QString& objectTypeField,
        State eventState = State::none);
    /**
     * Generates eventParameter's replacement for specific event, if it is applicable.
     */
    QString evaluateEventParameter(common::SystemContext* context,
        const AggregatedEventPtr& eventAggregator,
        const QString& eventParameter);

    /**
     * Generates html table with description of available event parameters.
     * By default, generates description only for visible in UI event parameters.
     */
    QString getHtmlDescription(bool skipHidden = true);

private:
    EventParameterHelper();
    virtual ~EventParameterHelper();
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::rules::utils

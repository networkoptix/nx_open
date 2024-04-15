// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

class LookupListEntriesModel::Private: public QObject
{
public:
    using Entry = std::map<QString, QString>;
    using Validator = std::function<bool(const QString&)>;
    using Formatter = std::function<QVariant(QString)>;

    QPointer<LookupListModel> data;
    std::optional<std::vector<int>> rowsIndexesToShow;
    QMap<QString, Validator> validatorByAttributeName;
    std::map<QString, Formatter> formatterByAttributeName;
    analytics::taxonomy::StateView* taxonomy;

    void initAttributeFunctions();
    static bool intValidator(const QString& value);
    static QVariant intFormatter(const QString& value);
    static QVariant doubleFormatter(const QString& value);
    static bool doubleValidator(const QString& value);
    static bool booleanValidator(const QString& value);
    static QVariant booleanFormatter(const QString& value);
    static QVariant stringFormatter(const QString& value);
    static QVariant objectFormatter(const QString& value);

    std::vector<int> getVisibleIndexes(const QString& filterText) const;
    QVariant getDisplayedValue(const QString& attributeName, const QString& value) const;
    QVector<int> removeIncorrectColumnValues();

    bool isValidValue(const QString& value, const QString& attributeName) const;
};

} // namespace nx::vms::client::desktop

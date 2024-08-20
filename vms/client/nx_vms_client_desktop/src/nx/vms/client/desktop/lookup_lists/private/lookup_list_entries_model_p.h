// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../lookup_list_entries_model.h"

namespace nx::vms::client::core::analytics::taxonomy { class Attribute; }

namespace nx::vms::client::desktop {

class LookupListEntriesModel::Private: public QObject
{
public:
    using Entry = std::map<QString, QString>;

    QPointer<LookupListModel> data;
    core::analytics::taxonomy::StateView* taxonomy = nullptr;

    void initAttributeFunctions();

    QVariant getDisplayValue(const QString& attributeName, const QString& value) const;
    QVariant getColorHexValue(const QString& attributeName, const QString& value) const;
    QVector<int> removeIncorrectColumnValues();
    bool isValidValue(const QString& value, const QString& attributeName) const;

private:
    using Validator = std::function<bool(const QString&)>;
    using Formatter = std::function<QVariant(QString)>;
    using RGBCalculator = std::function<QVariant(QString)>;

    QMap<QString, Validator> validatorByAttributeName;
    std::map<QString, Formatter> formatterByAttributeName;
    QMap<QString, RGBCalculator> rgbCalculatorByAttributeName;

    void initColorFunctions(
        const QString& fullAttributeName, const core::analytics::taxonomy::Attribute* attribute);
    void initEnumFunctions(
        const QString& fullAttributeName, const core::analytics::taxonomy::Attribute* attribute);
    void initNumberFunctions(
        const QString& fullAttributeName, const core::analytics::taxonomy::Attribute* attribute);
    static bool intValidator(const QString& value, const QVariant& min, const QVariant& max);
    static QVariant intFormatter(const QString& value);
    static QVariant doubleFormatter(const QString& value);
    static bool doubleValidator(const QString& value, const QVariant& min, const QVariant& max);
    static bool booleanValidator(const QString& value);
    static QVariant booleanFormatter(const QString& value);
    static QVariant stringFormatter(const QString& value);
    static QVariant objectFormatter(const QString& value);
};

} // namespace nx::vms::client::desktop

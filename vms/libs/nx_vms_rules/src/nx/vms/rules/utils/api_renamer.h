// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

#include <QtCore/QJsonValue>

namespace nx::vms::rules {

// Manage Id/Ids prefixes for complex fields.
class NX_VMS_RULES_API IdRenamer
{
public:
    IdRenamer();

    QString toApi(const QString& name) const;

private:
    void renameToPlural(const QString& source, QString target = {});

private:
    QMap<QString, QString> m_toApi;
};

struct NX_VMS_RULES_API UnitConverterBase
{
    virtual QString nameToApi(QString name) const { return name; }

    virtual QJsonValue valueToApi(const QJsonValue& value) const { return value; }
    virtual QJsonValue valueFromApi(const QJsonValue& value) const { return value; }
};

NX_VMS_RULES_API const UnitConverterBase* unitConverter(int type);
NX_VMS_RULES_API QString toApiFieldName(const QString& name, const QString& fieldId);
NX_VMS_RULES_API QString toApiFieldName(const QString& name, int fieldType);

} // namespace nx::vms::rules

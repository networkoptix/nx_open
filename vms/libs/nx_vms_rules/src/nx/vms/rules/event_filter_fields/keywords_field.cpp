// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "keywords_field.h"

#include <QtCore/QVariant>

#include <nx/vms/event/helpers.h>
#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

bool KeywordsField::match(const QVariant& value) const
{
    return m_list.empty() || nx::vms::event::checkForKeywords(value.toString(), m_list);
}

QString KeywordsField::string() const
{
    return m_string;
}

void KeywordsField::setString(const QString& string)
{
    if (m_string != string)
    {
        m_string = string;
        m_list = nx::vms::event::splitOnPureKeywords(m_string);
        emit stringChanged();
    }
}

QJsonObject KeywordsField::openApiDescriptor()
{
    return utils::constructOpenApiDescriptor<KeywordsField>();
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_filter_setup.h"

#include <QtQml/QtQml>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

void TextFilterSetup::registerQmlType()
{
    qmlRegisterUncreatableType<core::TextFilterSetup>("nx.vms.client.core", 1, 0,
        "TextFilterSetup", "Cannot create instance of TextFilterSetup");
}

TextFilterSetup::TextFilterSetup(QObject* parent):
    QObject(parent)
{
}

QString TextFilterSetup::text() const
{
    return m_text;
}

void TextFilterSetup::setText(const QString& value)
{
    const auto newText = value.trimmed();
    if (m_text == newText)
        return;

    m_text = newText;
    NX_VERBOSE(this, "Set filter text to \"%1\"", m_text);
    emit textChanged();
}

} // namespace nx::vms::client::core

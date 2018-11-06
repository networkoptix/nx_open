#include "basic_text_provider.h"

namespace nx::vms::client::desktop {

BasicTextProvider::BasicTextProvider(QObject* parent):
    base_type(parent)
{
}

BasicTextProvider::BasicTextProvider(const QString& text, QObject* parent):
    base_type(parent),
    m_text(text)
{
}

QString BasicTextProvider::text() const
{
    return m_text;
}

void BasicTextProvider::setText(const QString& value)
{
    if (m_text == value)
        return;

    m_text = value;
    emit textChanged(m_text);
}

} // namespace nx::vms::client::desktop

#include "provided_text_display.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

ProvidedTextDisplay::ProvidedTextDisplay(QObject* parent):
    QObject(parent)
{
}

ProvidedTextDisplay::ProvidedTextDisplay(AbstractTextProvider* textProvider, QObject* parent):
    ProvidedTextDisplay(parent)
{
    setTextProvider(textProvider);
}

AbstractTextProvider* ProvidedTextDisplay::textProvider() const
{
    return m_textProvider;
}

void ProvidedTextDisplay::setTextProvider(AbstractTextProvider* value)
{
    if (m_textProvider)
        m_textProvider->disconnect(this);

    m_textProvider = value;
    updateText();

    if (!m_textProvider)
        return;

    connect(m_textProvider.data(), &AbstractTextProvider::textChanged,
        this, &ProvidedTextDisplay::setText);

    connect(m_textProvider.data(), &QObject::destroyed, this,
        [this]() { setText(QString()); });
}

void ProvidedTextDisplay::updateText()
{
    setText(m_textProvider
        ? m_textProvider->text()
        : QString());
}

void ProvidedTextDisplay::setText(const QString& value)
{
    m_setter(value);
}

} // namespace nx::vms::client::desktop

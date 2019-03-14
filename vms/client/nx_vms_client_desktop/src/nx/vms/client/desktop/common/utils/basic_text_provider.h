#pragma once

#include "abstract_text_provider.h"

namespace nx::vms::client::desktop {

class BasicTextProvider: public AbstractTextProvider
{
    Q_OBJECT
    using base_type = AbstractTextProvider;

public:
    explicit BasicTextProvider(QObject* parent = nullptr);
    explicit BasicTextProvider(const QString& text, QObject* parent = nullptr);

    virtual QString text() const override;
    void setText(const QString& value);

private:
    QString m_text;
};

} // namespace nx::vms::client::desktop

#pragma once

#include <QtCore/QObject>

namespace nx::client::desktop {

class AnalyticsActionHandler: public QObject
{
    Q_OBJECT

public:
    explicit AnalyticsActionHandler(QObject* parent = nullptr);
    virtual ~AnalyticsActionHandler() override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop

#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_search_widget.h"

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class MotionSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~MotionSearchWidget() override;

    void setMediaWidget(QnMediaResourceWidget* value);

    QList<QRegion> filterRegions() const;

signals:
    void filterRegionsChanged(const QList<QRegion>& value);

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

#pragma once

#include <QtCore/QList>
#include <QtGui/QRegion>

#include "abstract_search_widget.h"

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class SimpleMotionSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    SimpleMotionSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~SimpleMotionSearchWidget() override;

    QList<QRegion> filterRegions() const;
    void setFilterRegions(const QList<QRegion>& value);

signals:
    void filterRegionsChanged(const QList<QRegion>& value);

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

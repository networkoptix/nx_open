#pragma once

#include "scroll_bar_proxy.h"

#include <QtWidgets/QScrollBar>

namespace nx::vms::client::desktop {

class SnappedScrollBar: public QScrollBar
{
    using base_type = QScrollBar;

public:
    explicit SnappedScrollBar(QWidget* parent = nullptr);
    explicit SnappedScrollBar(Qt::Orientation orientation, QWidget* parent = nullptr);
    virtual ~SnappedScrollBar() override;

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    bool useItemViewPaddingWhenVisible() const;
    void setUseItemViewPaddingWhenVisible(bool useItemViewPaddingWhenVisible);

    // If useMaximumSpace is true, snapped scroll bar length fills entire parent.
    // If false, snapped scroll bar aligns its length along original scroll bar.
    bool useMaximumSpace() const;
    void setUseMaximumSpace(bool useMaximumSpace);

    ScrollBarProxy* proxyScrollBar() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

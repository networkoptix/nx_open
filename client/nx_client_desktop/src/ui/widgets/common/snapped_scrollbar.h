#pragma once

#include <QtWidgets/QScrollBar>

#include <ui/widgets/common/scroll_bar_proxy.h>

class QnSnappedScrollBarPrivate;

class QnSnappedScrollBar : public QScrollBar
{
    typedef QScrollBar base_type;

public:
    explicit QnSnappedScrollBar(QWidget *parent = nullptr);
    explicit QnSnappedScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QnSnappedScrollBar();

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    bool useItemViewPaddingWhenVisible() const;
    void setUseItemViewPaddingWhenVisible(bool useItemViewPaddingWhenVisible);

    bool useMaximumSpace() const;
    void setUseMaximumSpace(bool useMaximumSpace);

    QnScrollBarProxy *proxyScrollBar() const;

private:
    QScopedPointer<QnSnappedScrollBarPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnSnappedScrollBar)
};

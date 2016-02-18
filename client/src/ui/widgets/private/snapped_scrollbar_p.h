#pragma once

#include <ui/widgets/scroll_bar_proxy.h>

class QnSnappedScrollBar;

class QnSnappedScrollBarPrivate : public QObject
{
    QnSnappedScrollBar *q_ptr;
    Q_DECLARE_PUBLIC(QnSnappedScrollBar)

public:
    QnSnappedScrollBarPrivate(QnSnappedScrollBar *parent);

    void updateGeometry();
    void updateProxyScrollBarSize();

    bool eventFilter(QObject *object, QEvent *event);

public:
    QnScrollBarProxy *proxyScrollbar;
    Qt::Alignment alignment;
    bool useHeaderShift;
    bool useItemViewPaddingWhenVisible;
};

#pragma once

#include <QtCore/QPointer>

#include <ui/widgets/common/scroll_bar_proxy.h>

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
    QPointer<QnScrollBarProxy> proxyScrollbar;
    Qt::Alignment alignment;
    bool useItemViewPaddingWhenVisible;
    bool useMaximumSpace;
};

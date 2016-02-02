#pragma once

#include <QCommonStyle>
#include <private/qcommonstyle_p.h>

#include <ui/style/generic_palette.h>
#include <ui/style/noptix_style_animator.h>

class QnNxStylePrivate : public QCommonStylePrivate {
    Q_DECLARE_PUBLIC(QnNxStyle)

public:
    QnNxStylePrivate();

    QnGenericPalette palette;
    QnNoptixStyleAnimator *idleAnimator;
};

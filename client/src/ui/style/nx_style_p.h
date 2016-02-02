#pragma once

#include <QCommonStyle>
#include <private/qcommonstyle_p.h>

#include <ui/style/generic_palette.h>

class QnNxStylePrivate : public QCommonStylePrivate {
    Q_DECLARE_PUBLIC(QnNxStyle)

public:
    QnNxStylePrivate();

    QnGenericPalette palette;
};

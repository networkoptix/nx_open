#pragma once

#include <QtGui/QIcon>
#include <QtCore/QVector>

class QnIcon: public QIcon
{
public:
    static constexpr Mode Error = static_cast<Mode>(0xE);
    static constexpr Mode Pressed = static_cast<Mode>(0xF);

    using SuffixesList = QVector<QPair<Mode, QString>>;
};

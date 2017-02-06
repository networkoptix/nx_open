#pragma once

class QnIcon: public QIcon
{
public:
    static const QIcon::Mode Pressed;

    using SuffixesList = QVector<QPair<QIcon::Mode, QString>>;
};

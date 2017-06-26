#pragma once

class QnIcon: public QIcon
{
public:
    static const QIcon::Mode Pressed;
    static const QIcon::Mode Error;

    using SuffixesList = QVector<QPair<QIcon::Mode, QString>>;
};

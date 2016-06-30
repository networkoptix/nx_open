#pragma once

#include <QtCore/QMetaType>

class QnIndents
{
public:
    QnIndents();
    QnIndents(int left, int right);
    explicit QnIndents(int indent);

    int left() const;
    void setLeft(int value);

    int right() const;
    void setRight(int value);

protected:
    int m_left, m_right;
};

Q_DECLARE_METATYPE(QnIndents);

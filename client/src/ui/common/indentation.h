#pragma once

#include <QtCore/QMetaType>

class QnIndentation
{
public:
    QnIndentation();
    QnIndentation(int indent);
    QnIndentation(int left, int right);

    int left() const;
    void setLeft(int value);

    int right() const;
    void setRight(int value);

protected:
    int m_left, m_right;
};

Q_DECLARE_METATYPE(QnIndentation);

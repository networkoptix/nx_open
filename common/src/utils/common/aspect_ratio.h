#pragma once

#include <QtCore/QSize>
#include <QtCore/QMetaType>

class QnAspectRatio
{
public:
    QnAspectRatio();
    QnAspectRatio(int width, int height);
    explicit QnAspectRatio(const QSize& size);

    bool isValid() const;

    int width() const;
    int height() const;

    float toFloat() const;
    QString toString() const;
    QString toString(const QString &format) const;

    static QList<QnAspectRatio> standardRatios();
    static QnAspectRatio closestStandardRatio(float aspectRatio);
    static const int kInvalidAspectRatio = -1;

    static bool isRotated90(qreal angle);

    bool operator ==(const QnAspectRatio &other) const;

private:
    int m_width = 0;
    int m_height = 0;
};

Q_DECLARE_METATYPE(QnAspectRatio)

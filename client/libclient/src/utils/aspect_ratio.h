#pragma once

class QnAspectRatio
{
public:
    QnAspectRatio();
    QnAspectRatio(int width, int height);

    bool isValid() const;

    int width() const;
    int height() const;

    float toFloat() const;
    QString toString() const;
    QString toString(const QString &format) const;

    static QList<QnAspectRatio> standardRatios();
    static QnAspectRatio closestStandardRatio(float aspectRatio);

    static bool isRotated90(qreal angle);

    bool operator ==(const QnAspectRatio &other) const;

private:
    int m_width;
    int m_height;
};

Q_DECLARE_METATYPE(QnAspectRatio)

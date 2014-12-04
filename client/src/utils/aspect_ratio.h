#ifndef ASPECT_RATIO_H
#define ASPECT_RATIO_H

#include <QtCore/QString>

#include <ui/actions/actions.h>

class QnAspectRatio
{
public:
    QnAspectRatio(int width = 1, int height = 1);

    float toFloat() const;
    QString toString() const;

    static QList<QnAspectRatio> standardRatios();
    static QnAspectRatio closestStandardRatio(float aspectRatio);

    static bool isRotated90(qreal angle);
    static Qn::ActionId aspectRatioActionId(const QnAspectRatio &aspectRatio);

    bool operator ==(const QnAspectRatio &other) const;

private:
    int m_width;
    int m_height;
};

#endif // ASPECT_RATIO_H

#include "aspect_ratio.h"

namespace {
    const QList<QnAspectRatio> standardRatios =
            QList<QnAspectRatio>()
                << QnAspectRatio(4, 3)
                << QnAspectRatio(16, 9)
                << QnAspectRatio(3, 4)
                << QnAspectRatio(9, 16);
    const QList<Qn::ActionId> ratioActions =
            QList<Qn::ActionId>()
                << Qn::SetCurrentLayoutAspectRatio4x3Action
                << Qn::SetCurrentLayoutAspectRatio16x9Action
                << Qn::SetCurrentLayoutAspectRatio3x4Action
                << Qn::SetCurrentLayoutAspectRatio9x16Action;
}

QnAspectRatio::QnAspectRatio(int width, int height) :
    m_width(width),
    m_height(height)
{}

qreal QnAspectRatio::toReal() const {
    return static_cast<qreal>(m_width) / m_height;
}

QString QnAspectRatio::toString() const {
    return lit("%1:%2").arg(m_width).arg(m_height);
}

QList<QnAspectRatio> QnAspectRatio::standardRatios() {
    return ::standardRatios;
}

QnAspectRatio QnAspectRatio::closestStandardRatio(qreal aspectRatio) {
    QnAspectRatio closest = ::standardRatios.first();
    qreal diff = qAbs(aspectRatio - ::standardRatios.first().toReal());

    foreach (const QnAspectRatio &ratio, ::standardRatios) {
        qreal d = qAbs(aspectRatio - ratio.toReal());
        if (d < diff) {
            diff = d;
            closest = ratio;
        }
    }

    return closest;
}

bool QnAspectRatio::isRotated90(qreal angle) {
    return qAbs(90 - qAbs(angle)) < 45;
}

Qn::ActionId QnAspectRatio::aspectRatioActionId(const QnAspectRatio &aspectRatio) {
    int index = ::standardRatios.indexOf(aspectRatio);
    if (index == -1)
        index = ::standardRatios.indexOf(closestStandardRatio(aspectRatio.toReal()));
    return ratioActions[index];
}

bool QnAspectRatio::operator ==(const QnAspectRatio &other) const {
    return m_width == other.m_width && m_height == other.m_height;
}

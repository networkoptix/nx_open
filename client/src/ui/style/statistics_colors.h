#ifndef QN_STATISTICS_COLORS_H
#define QN_STATISTICS_COLORS_H

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/json.h>

class QnStatisticsColors {
public:
    QnStatisticsColors();
    QnStatisticsColors(const QnStatisticsColors &other);
    ~QnStatisticsColors();

    QColor grid;
    QColor frame;
    QColor cpu;
    QColor ram;
    QColor networkLimit;
    QVector<QColor> hdds;
    QVector<QColor> network;

    QColor hddByKey(const QString &key) const;
    QColor networkByKey(const QString &key) const;

    void update(const QByteArray &serializedValue);

private:
    /**
     * @brief ensureHdds        Make sure all color arrays contain at least one element.
     *                          Fill with default values if empty.
     */
    void ensureVectors();
};

Q_DECLARE_METATYPE(QnStatisticsColors)

QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnStatisticsColors)

#endif // QN_STATISTICS_COLORS_H

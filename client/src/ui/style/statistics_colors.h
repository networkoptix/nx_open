#ifndef STATISTICS_COLORS_H
#define STATISTICS_COLORS_H

#include <QColor>
#include <QtCore/QMetaType>

#include <utils/common/json.h>

class QnStatisticsColors
{
public:
    QnStatisticsColors();
    QnStatisticsColors(const QnStatisticsColors &source);
    ~QnStatisticsColors();

    QColor grid;
    QColor frame;
    QColor cpu;
    QColor ram;
    QColor networkLimit;
    QVector<QColor> hdds;
    QVector<QColor> networkIn;
    QVector<QColor> networkOut;

    QColor hddByKey(const QString &key) const;
    QColor networkInByKey(const QString &key) const;
    QColor networkOutByKey(const QString &key) const;

    void update(const QByteArray &serializedValue);
private:

    /**
     * @brief ensureHdds        Make sure all color arrays contain at least one element.
     *                          Fill by default values if empty.
     */
    void ensureVectors();
};

Q_DECLARE_METATYPE(QnStatisticsColors)

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStatisticsColors, (grid)(frame)(cpu)(ram)(hdds), inline)


#endif // STATISTICS_COLORS_H

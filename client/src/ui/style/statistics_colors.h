#ifndef STATISTICS_COLORS_H
#define STATISTICS_COLORS_H

#include <QColor>

class QnStatisticsColors
{
public:
    QnStatisticsColors();
    QnStatisticsColors(const QnStatisticsColors &source);
    ~QnStatisticsColors();

    QColor grid() const;
    void setGrid(const QColor& value);

    QColor frame() const;
    void setFrame(const QColor& value);

    QColor cpu() const;
    void setCpu(const QColor& value);

    QColor ram() const;
    void setRam(const QColor& value);

    QColor hdd(const QString &key) const;
    QVector<QColor> hdds() const;

    void update(const QString &serializedValue);
private:
    QColor m_grid;
    QColor m_frame;
    QColor m_cpu;
    QColor m_ram;
    QVector<QColor> m_hdds;

};

Q_DECLARE_METATYPE(QnStatisticsColors)
/*
qRegisterMetaType<QnStatisticsColors>();
qRegisterMetaTypeStreamOperators<QnStatisticsColors>();
*/


#endif // STATISTICS_COLORS_H

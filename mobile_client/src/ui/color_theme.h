#ifndef COLOR_THEME_H
#define COLOR_THEME_H

#include <QtCore/QObject>
#include <QtGui/QColor>

class QnColorTheme : public QObject {
    Q_OBJECT
public:
    explicit QnColorTheme(QObject *parent = 0);

    void readFromFile(const QString &fileName);

    Q_INVOKABLE QColor color(const QString &key) const;

private:
    QHash<QString, QColor> m_colors;
};

#endif // COLOR_THEME_H

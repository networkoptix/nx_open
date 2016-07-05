#ifndef QN_PALETTE_WIDGET_H
#define QN_PALETTE_WIDGET_H

#include <QtWidgets/QTableWidget>

class QnPaletteWidget: public QTableWidget {
    Q_OBJECT;

    typedef QTableWidget base_type;

public:
    QnPaletteWidget(QWidget *parent = NULL);
    virtual ~QnPaletteWidget();

    const QPalette &displayPalette() const;
    void setDisplayPalette(const QPalette &displayPalette);

private:
    QPalette m_displayPalette;
};

#endif // QN_PALETTE_WIDGET_H


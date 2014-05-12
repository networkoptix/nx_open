#ifndef SEARCH_LINE_EDIT_H
#define SEARCH_LINE_EDIT_H

#include <QtWidgets/QLineEdit>

class QnSearchButton;

/**
    Clear button on the right hand side of the search widget.
    Hidden by default
    "A circle with an X in it"
 */
class QnClearButton : public QAbstractButton {
    Q_OBJECT
public:
    QnClearButton(QWidget *parent = 0);
    void paintEvent(QPaintEvent *event);
public slots:
    void textChanged(const QString &text);
};

class QnSearchLineEdit : public QWidget
{
    Q_OBJECT
public:
    explicit QnSearchLineEdit(QWidget *parent = 0);

    inline QLineEdit *lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
protected:
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void inputMethodEvent(QInputMethodEvent *e);
    bool event(QEvent *event);

protected:
    void updateGeometries();
    void initStyleOption(QStyleOptionFrameV2 *option) const;

    QLineEdit *m_lineEdit;
    QnClearButton *m_clearButton;
    QnSearchButton *m_searchButton;
};

#endif // SEARCH_LINE_EDIT_H

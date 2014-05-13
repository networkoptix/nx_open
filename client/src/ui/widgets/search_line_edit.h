#ifndef SEARCH_LINE_EDIT_H
#define SEARCH_LINE_EDIT_H

#include <QtWidgets/QLineEdit>

class QnSearchLineEdit : public QWidget
{
    Q_OBJECT
public:
    explicit QnSearchLineEdit(QWidget *parent = 0);

    inline QLineEdit *lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

signals:
    void prevButtonClicked();
    void nextButtonClicked();
    void textChanged(const QString &text);
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

    QLineEdit* m_lineEdit;
    QLabel* m_occurencesLabel;
    QAbstractButton *m_prevButton;
    QAbstractButton *m_nextButton;
    QAbstractButton *m_searchButton;
};

#endif // SEARCH_LINE_EDIT_H

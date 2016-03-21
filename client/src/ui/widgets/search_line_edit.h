#ifndef SEARCH_LINE_EDIT_H
#define SEARCH_LINE_EDIT_H

#include <QtWidgets/QLineEdit>

class QTimer;

class QnSearchLineEdit : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int textChangedSignalFilterMs READ textChangedSignalFilterMs WRITE setTextChangedSignalFilterMs)

public:
    explicit QnSearchLineEdit(QWidget *parent = 0);

    inline QLineEdit *lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    int textChangedSignalFilterMs() const;
    void setTextChangedSignalFilterMs(int filterMs);

signals:
    void textChanged(const QString &text);
    void escKeyPressed();
    void enterKeyPressed();
    void enabledChanged();

protected:
    void resizeEvent(QResizeEvent *event);
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event);

protected:
    void initStyleOption(QStyleOptionFrameV2 *option) const;

private:
    typedef QSharedPointer<QTimer> QTimerPtr;

    QLineEdit* m_lineEdit;
    int m_textChangedSignalFilterMs;
    QTimerPtr m_filterTimer;
};

#endif // SEARCH_LINE_EDIT_H

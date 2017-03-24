#pragma once

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleOption>

class QTimer;

class QnSearchLineEdit : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int textChangedSignalFilterMs READ textChangedSignalFilterMs WRITE setTextChangedSignalFilterMs)

public:
    explicit QnSearchLineEdit(QWidget *parent = 0);

    inline QLineEdit *lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;
    QString text() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    int textChangedSignalFilterMs() const;
    void setTextChangedSignalFilterMs(int filterMs);

    void clear();
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

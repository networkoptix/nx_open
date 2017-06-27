#pragma once

#include <QtWidgets/QPlainTextEdit>

/*
* QLabel supports text wrapping at word boundary but not any arbitrary place.
* This QTextEdit descendant serves to overcome that limitation.
*/
class QnTextEditLabel : public QTextEdit
{
    Q_OBJECT
    using base_type = QTextEdit;

public:
    QnTextEditLabel(QWidget* parent = nullptr);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    bool isAutoWrapped() const;

private:
    QSize m_documentSize;
};

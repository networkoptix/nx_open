#ifndef QN_PROXY_LABEL_H
#define QN_PROXY_LABEL_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QGraphicsProxyWidget>

#include <utils/common/connective.h>

class QLabel;

/**
 * A <tt>QLabel</tt> wrapped in <tt>QGraphicsProxyWidget</tt>. Presents an
 * interface similar to that of <tt>QLabel</tt> and fixes some problems of 
 * <tt>QGraphicsProxyWidget</tt>.
 * 
 * See https://bugreports.qt-project.org/browse/QTBUG-14622.
 */
class QnProxyLabel: public Connective<QGraphicsProxyWidget> {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
    Q_PROPERTY(int indent READ indent WRITE setIndent)
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)
    Q_PROPERTY(Qt::TextInteractionFlags textInteractionFlags READ textInteractionFlags WRITE setTextInteractionFlags)
    Q_PROPERTY(bool hasSelectedText READ hasSelectedText)
    Q_PROPERTY(QString selectedText READ selectedText)
    
    typedef Connective<QGraphicsProxyWidget> base_type;

public:
    explicit QnProxyLabel(const QString &text, QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    explicit QnProxyLabel(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnProxyLabel();

    QString text() const;

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat textFormat);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    bool wordWrap() const;
    void setWordWrap(bool wordWrap);

    int indent() const;
    void setIndent(int indent);

    int margin() const;
    void setMargin(int margin);

    bool openExternalLinks() const;
    void setOpenExternalLinks(bool openExternalLinks);

    Qt::TextInteractionFlags textInteractionFlags() const;
    void setTextInteractionFlags(Qt::TextInteractionFlags flags);

    void setSelection(int start, int length);
    bool hasSelectedText() const;
    QString selectedText() const;
    int selectionStart() const;

    virtual bool eventFilter(QObject *object, QEvent *event) override;

public slots:
    void setText(const QString &text);
    void setNum(int number);
    void setNum(double number);
    void setPixmap(const QPixmap &pixmap);
    void setPicture(const QPicture &picture);
    void clear();

signals:
    void linkActivated(const QString &link);
    void linkHovered(const QString &link);

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private:
    void init();

private:
    QScopedPointer<QLabel> m_label;
};


class QnDebugProxyLabel: public QnProxyLabel {
    Q_OBJECT
public:
    explicit QnDebugProxyLabel(const QString &text, QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        QnProxyLabel(text, parent, windowFlags) {init();}
    explicit QnDebugProxyLabel(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        QnProxyLabel(parent, windowFlags) {init();}
    void setMessagesLimit(int limit);
    void appendTextQueued(const QString &text);
    void appendText(const QString &text);
signals:
    void textAppended(const QString &text);
private:
    void init();

private:
    int m_messagesLimit;
};

#endif // QN_PROXY_LABEL_H

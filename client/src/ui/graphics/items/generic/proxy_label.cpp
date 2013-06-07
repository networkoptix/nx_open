
#include "proxy_label.h"

#include <QtGui/QLabel>
#include <QtGui/QLayout>


QnProxyLabel::QnProxyLabel(const QString &text, QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags)
{
    init();
    setText(text);
}

QnProxyLabel::QnProxyLabel(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags)
{
    init();
}

void QnProxyLabel::init() {
    m_label.reset(new QLabel());
    setWidget(m_label.data());

    connect(m_label, SIGNAL(linkActivated(const QString &)), this, SIGNAL(linkActivated(const QString &)));
    connect(m_label, SIGNAL(linkHovered(const QString &)), this, SIGNAL(linkHovered(const QString &)));

    setCacheMode(ItemCoordinateCache);
}

QnProxyLabel::~QnProxyLabel() {
    /* setWidget calls back into the scene, which may result in accesses to parent item,
     * which in turn may be being destroyed at the moment, ultimately leading to a crash
     * due to pure virtual function call. So we clean up as much state as possible before
     * setWidget call. Note that without this call we get stack overflow. */

    setVisible(false);
    setParentItem(NULL);
    setWidget(NULL);
}

QString QnProxyLabel::text() const {
    return m_label->text();
}

void QnProxyLabel::setText(const QString &text) {
    if(text == this->text())
        return;

    m_label->setText(text);

    updateGeometry();
}

void QnProxyLabel::setNum(int number) {
    setText(QString::number(number));
}

void QnProxyLabel::setNum(double number) {
    setText(QString::number(number));
}

void QnProxyLabel::setPixmap(const QPixmap &pixmap) {
    m_label->setPixmap(pixmap);
    updateGeometry();
}

void QnProxyLabel::setPicture(const QPicture &picture) {
    m_label->setPicture(picture);
    updateGeometry();
}

void QnProxyLabel::clear() {
    m_label->clear();
    updateGeometry();
}

Qt::TextFormat QnProxyLabel::textFormat() const {
    return m_label->textFormat();
}

void QnProxyLabel::setTextFormat(Qt::TextFormat textFormat) {
    if(textFormat == this->textFormat())
        return;

    m_label->setTextFormat(textFormat);

    updateGeometry();
}

Qt::Alignment QnProxyLabel::alignment() const {
    return m_label->alignment();
}

void QnProxyLabel::setAlignment(Qt::Alignment alignment) {
    if(alignment == this->alignment())
        return;

    m_label->setAlignment(alignment);

    updateGeometry();
}

bool QnProxyLabel::wordWrap() const {
    return m_label->wordWrap();
}

void QnProxyLabel::setWordWrap(bool wordWrap) {
    if(wordWrap == this->wordWrap())
        return;

    m_label->setWordWrap(wordWrap);

    updateGeometry();
}

int QnProxyLabel::indent() const {
    return m_label->indent();
}

void QnProxyLabel::setIndent(int indent) {
    if(indent == this->indent())
        return;

    m_label->setIndent(indent);

    updateGeometry();
}

int QnProxyLabel::margin() const {
    return m_label->margin();
}

void QnProxyLabel::setMargin(int margin) {
    if(margin == this->margin())
        return;

    m_label->setMargin(margin);

    updateGeometry();
}

bool QnProxyLabel::openExternalLinks() const {
    return m_label->openExternalLinks();
}

void QnProxyLabel::setOpenExternalLinks(bool openExternalLinks) {
    m_label->setOpenExternalLinks(openExternalLinks);
}

Qt::TextInteractionFlags QnProxyLabel::textInteractionFlags() const {
    return m_label->textInteractionFlags();
}

void QnProxyLabel::setTextInteractionFlags(Qt::TextInteractionFlags flags) {
    m_label->setTextInteractionFlags(flags);
}

void QnProxyLabel::setSelection(int start, int length) {
    m_label->setSelection(start, length);
}

bool QnProxyLabel::hasSelectedText() const {
    return m_label->hasSelectedText();
}

QString QnProxyLabel::selectedText() const {
    return m_label->selectedText();
}

int QnProxyLabel::selectionStart() const {
    return m_label->selectionStart();
}

QSizeF QnProxyLabel::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    /* QGraphicsProxyWidget doesn't take heightForWidth into account.
     * So we implement sizeHint our own way. */

    QWidget *widget = this->widget();
    if (!widget)
        return QGraphicsWidget::sizeHint(which, constraint);

    QSizeF result;
    switch (which) {
    case Qt::PreferredSize:
        if (QLayout *layout = widget->layout()) {
            result = layout->sizeHint();
        } else {
            result = widget->sizeHint();

            if(constraint.width() > 0 && widget->sizePolicy().hasHeightForWidth()) {
                int height = widget->heightForWidth(constraint.width());
                if(height > 0) {
                    result.setWidth(constraint.width());
                    result.setHeight(height);
                }
            }
        }
        break;
    case Qt::MinimumSize:
        if (QLayout *layout = widget->layout()) {
            result = layout->minimumSize();
        } else {
            result = widget->minimumSizeHint();

            if(constraint.width() > 0 && widget->sizePolicy().hasHeightForWidth()) {
                int height = widget->heightForWidth(constraint.width());
                if(height > 0) {
                    result.setWidth(constraint.width());
                    result.setHeight(height);
                }
            }
        }
        break;
    case Qt::MaximumSize:
        if (QLayout *layout = widget->layout()) {
            result = layout->maximumSize();
        } else {
            result = QSizeF(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        break;
    case Qt::MinimumDescent:
        result = constraint;
        break;
    default:
        break;
    }

    return result;
}

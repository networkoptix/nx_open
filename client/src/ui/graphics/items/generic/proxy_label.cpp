#include "proxy_label.h"

#include <QtGui/QLabel>

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
    return;
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

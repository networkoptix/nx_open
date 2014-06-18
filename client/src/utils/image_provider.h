#ifndef IMAGE_PROVIDER_H
#define IMAGE_PROVIDER_H

#include <QtCore/QObject>
#include <QtGui/QImage>

class QnImageProvider: public QObject {
    Q_OBJECT

public:
    QnImageProvider(QObject *parent = 0): QObject(parent) {}
    virtual ~QnImageProvider() {}

    virtual QImage image() const = 0;

public slots:
    void loadAsync() {
        doLoadAsync();
    }

signals:
    void imageChanged(const QImage &image);

protected:
    virtual void doLoadAsync() = 0;

};

class QnBasicImageProvider: public QnImageProvider {
    Q_OBJECT
public:
    QnBasicImageProvider(const QImage &image, QObject *parent = 0):
        QnImageProvider(parent),
        m_image(image)
    {
        connect(this, &QnBasicImageProvider::loadDelayed, this, &QnImageProvider::imageChanged, Qt::QueuedConnection);
    }
    virtual ~QnBasicImageProvider() {}
    virtual QImage image() const override { return m_image; }
signals:
    void loadDelayed(const QImage &image);
protected:
    virtual void doLoadAsync() { emit loadDelayed(m_image); }
private:
    QImage m_image;

};

#endif // IMAGE_PROVIDER_H

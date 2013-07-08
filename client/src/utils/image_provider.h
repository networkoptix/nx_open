#ifndef IMAGE_PROVIDER_H
#define IMAGE_PROVIDER_H

#include <QtCore/QObject>
#include <QtGui/QImage>

class QnImageProvider: public QObject {
    Q_OBJECT

public:
    QnImageProvider(QObject *parent = 0):
        QObject(parent) {}
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

#endif // IMAGE_PROVIDER_H

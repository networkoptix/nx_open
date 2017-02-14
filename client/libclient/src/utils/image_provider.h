#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

class QnImageProvider: public QObject
{
    Q_OBJECT

public:
    QnImageProvider(QObject* parent = nullptr);
    virtual ~QnImageProvider() {}

    virtual QImage image() const = 0;

public slots:
    void loadAsync();

signals:
    void imageChanged(const QImage& image);

protected:
    virtual void doLoadAsync() = 0;

};

class QnBasicImageProvider: public QnImageProvider
{
    Q_OBJECT
public:
    QnBasicImageProvider(const QImage& image, QObject* parent = nullptr);

    virtual ~QnBasicImageProvider() {}
    virtual QImage image() const override;

protected:
    virtual void doLoadAsync() override;

private:
    QImage m_image;
};

#include "mainwindow.h"

#include <QtGui/QApplication>
#include <QFile>
#include <QDebug>

void rtspTest()
{
    QFile file("c:/1111/binary_raw.rtsp");
    file.open(QFile::ReadOnly);
    QByteArray data = file.readAll();
    const unsigned char* buff = (const unsigned char*) data.data() + 0x1ea;
    const unsigned char* buffStart = buff;
    const unsigned char* bufEnd = (const unsigned char*) buff + data.size();
    while (buff < bufEnd)
    {
        int len = (buff[2] << 8) + buff[3]; 
        qDebug() << "marker=" << buff[0] << "track=" << (int)buff[1] << "len=" << len;
        buff += len + 4;
    }
}

int main(int argc, char **argv)
{
    rtspTest();

    Q_INIT_RESOURCE(images);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    MainWindow window;
    window.show();

    return app.exec();
}

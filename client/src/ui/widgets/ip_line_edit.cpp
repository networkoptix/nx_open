#include "ip_line_edit.h"

#include <QtGui/QValidator>
#include <QtGui/QKeyEvent>

#define DOT QLatin1Char('.')

class IpAddressValidator: public QValidator{
    virtual QValidator::State validate(QString &input, int &pos) const override{
        Q_UNUSED(pos)

        QStringList sections = input.split(QLatin1Char('.'));
        int s = sections.size();
        if(s > 4){
            return Invalid;
        }

        bool emptyGroup = false;
        foreach(QString section, sections){
            bool ok;
            if(section.isEmpty()){
                emptyGroup = true;
                continue;
            }
            int val = section.toInt(&ok);
            if(!ok || val<0 || val>255){
                return Invalid;
            }
        }

        if (s < 4 || emptyGroup){
            return Intermediate;
        }

        return Acceptable;
    }

    virtual void fixup(QString &input) const override{
        QStringList sections = input.split(DOT);
        while(sections.size() > 4)
            sections.removeLast();
        while (sections.size() < 4)
            sections.append(QLatin1String("0"));

        for (int i = 0; i < 4; i++){
            if (sections[i].isEmpty()){
                sections[i] = QLatin1String("0");
            } else {
                bool ok;
                int val = sections[i].toInt(&ok);
                if(!ok || val<0)
                    sections[i] = QLatin1String("0");
                else if (val>255)
                    sections[i] = QLatin1String("255");
            }
        }
        input.clear();
        for (int i = 0; i < 3; i++){
            input.append(sections[i]);
            input.append(DOT);
        }
        input.append(sections[3]);
    }

};


QnIpLineEdit::QnIpLineEdit(QWidget *parent):
    QLineEdit(parent){
    setValidator(new IpAddressValidator());
    setText(QLatin1String("127.0.0.1"));
}

void QnIpLineEdit::keyPressEvent(QKeyEvent *event){
    if (event->key() >= QLatin1Char('0') && event->key() <= QLatin1Char('9')){
        QLineEdit::keyPressEvent(event);

        QString input = text();
        QStringList sections = input.split(DOT);
        QString last = sections.last();

        bool ok;
        int val = last.toInt(&ok);
        if (sections.count() < 4  && ok && val*10 > 255)
            setText(input + DOT);
    }else if (event->key() == 32 || event->key() == DOT){
        QString input = text();
        if (input.at(input.size() - 1) != DOT){
            QStringList sections = input.split(DOT);
            if (sections.count() < 4)
                setText(input + DOT);
        }
        event->setAccepted(true);
    } else
        QLineEdit::keyPressEvent(event);
}

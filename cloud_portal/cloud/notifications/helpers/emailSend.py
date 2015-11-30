import smtplib

from email.mime.text import MIMEText
from email.Header import Header
from email.MIMEMultipart import MIMEMultipart

from flask import current_app

def sendEmail(message,subject,recipients = [],cc=[], bcc = [],separateMails = False):
    try:
        config = current_app.app_config

        if separateMails:
            for recipient in recipients + cc + bcc:
                sendSingleEmail(message,subject,recipient)
            return True

        multipart = MIMEMultipart('alternative')
        multipart['Subject'] = Header(subject.encode('utf-8'), 'UTF-8').encode()
        multipart['To'] = Header(", ".join(recipients).encode('utf-8'), 'UTF-8').encode()
        multipart['Cc'] = Header(", ".join(cc).encode('utf-8'), 'UTF-8').encode()
        multipart['From'] = Header(config["emails"]["mailFrom"].encode('utf-8'), 'UTF-8').encode()

        htmlpart = MIMEText(message.encode('utf-8'), 'html', 'UTF-8')
        multipart.attach(htmlpart)

        sendContent(multipart.as_string(),recipients + cc + bcc)
        return True
    except:
        return False

def sendSingleEmail(message,subject,recipient):
    multipart = MIMEMultipart('alternative')

    multipart['Subject'] = Header(subject.encode('utf-8'), 'UTF-8').encode()
    multipart['To'] = Header(recipient.encode('utf-8'), 'UTF-8').encode()
    htmlpart = MIMEText(message.encode('utf-8'), 'html', 'UTF-8')
    multipart.attach(htmlpart)

    sendContent(multipart.as_string(),[recipient])

    return True


def sendContent(content,recipients):
    config = current_app.app_config

    server = smtplib.SMTP(config["emails"]["smtp"],config["emails"]["smtpPort"])
    server.ehlo()
    server.starttls()
    server.login(config["emails"]["login"], config["emails"]["password"])
    server.sendmail(config["emails"]["mailFrom"], recipients, content)
    server.close()
    return True

from notifications.helpers.emailSend import sendEmail
import codecs
import pystache


def send(email, msg_type, message):
    # 1. get


    message_template = read_template(msg_type)
    message_body = pystache.render(message_template, {"message": message})

    email_template = read_template('email_body')
    email_body = pystache.render(email_template, {"message_body": message_body})

    sendEmail(email_body, notifications_config[msg_type]['subject'], email)
    return True



def read_template(name):
    filename = 'static/templates/{0}.mustache'.format(name)
    #filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
    with codecs.open(filename,'r', 'utf-8') as stream:
        return stream.read()
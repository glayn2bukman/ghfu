import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEBase import MIMEBase
from email import Encoders
from email.mime.text import MIMEText

import os

smtp_server = ("smtp.gmail.com", 587) # the gmail smtp server
#account = ("jermapi42@gmail.com", "aA1jermapi")
account = ("jermapi43@gmail.com", "aA1jermapi")

def connect():
    global server
    server = smtplib.SMTP(*smtp_server)
    server.starttls()
    server.login(*account)

def send_mail(subject, files, mail_to, body=""):
    """
    mail_to -> list of recipients
    """

    if isinstance(mail_to, str): mail_to = [mail_to]
    if isinstance(files, str): files = [files]
    body = str(body)

    msg = MIMEMultipart()
    msg['Subject'] = subject
    msg['From'] = account[0]
    msg['To'] = ', '.join(mail_to)

    for _file in files:
        fname = os.path.split(_file)[-1]
        part = MIMEBase('application', "octet-stream")
        part.set_payload(open(_file, "rb").read())
        Encoders.encode_base64(part)

        part.add_header('Content-Disposition', 'attachment; filename=\"{}\"'.format(fname))

        msg.attach(part)

    part2 = MIMEText(body, "plain")
    msg.attach(part2)

    connect() # connect to smtp server...
    try:
        server.sendmail(account[0], mail_to, msg.as_string())
    except:
        connect()
        send_mail(subject, files, mail_to, body)


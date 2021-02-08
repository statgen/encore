import smtplib
from email.mime.text import MIMEText
from flask import current_app, g, url_for
from .job import Job

def get_notifier():
    if "notifier" not in g:
        g.notifier = Notifier(current_app.config)
    return g.notifier

class Notifier:
    def __init__(self, config):
        self.smtp_server = config.get("SMTP_SERVER", 'localhost')
        self.help_email = config.get("HELP_EMAIL", None)
        self.from_email = config.get("FROM_EMAIL", "do-not-reply@encore.sph.umich.edu")

    def send_user_feedback(self, user_email, user_fullname, user_message, from_page, user):
        to_address = self.help_email 
        if not to_address:
            raise Exception("HELP EMAIL NOT CONFIGURED") 
        if not user_message:
            raise Exception("EMPTY MESSAGE") 
        user_id = user.rid
        message = user_message + \
            "\n\nUser Info:\n" + \
            "Name: {}\nEmail: {}\nID:{}".format(user_fullname, user_email, user_id) +  \
            "\n\nReferring Page:\n" + \
            "\n"
        subject = "Encore User help ({})".format(user_fullname)
        self.send_mail(to_address, subject, message, {"reply-to": user_email})

        #user_email, user_fullname, from_page,current_user,message, message2,Q1,Q2,Q3,Q4

    def send_user_feedback2(self, user_email, user_fullname, from_page, user,message3,message2,Q1,Q2,Q3,Q4):
        to_address = self.help_email
        if not to_address:
            raise Exception("HELP EMAIL NOT CONFIGURED")
        user_id = user.rid
        message = '' + \
                  "\n\nUser Info:\n" + \
                  "Name: {}\nEmail: {}\nID:{} \n".format(user_fullname, user_email, user_id) + \
                  "Have you visited website before?: {}\n".format(Q1) + \
                  "How was your experince with job submission?: {} \nOverall, can you please rate the content of the website?: {}\n".format(Q2, Q3) + \
                  "How was your experince accessing/sharing the results?: {} \n".format(Q4) + \
                  "What could we make easier to find?: {}\nPlease share any additional feedback that could help us improve our site experience?:{}\n".format(message3, message2) + \
                  "\n\nReferring Page:\n" + \
                  "\n"
        subject = "Encore User Feedback ({})".format(user_fullname)
        self.send_feedback_mail(to_address, subject, message, {"reply-to": user_email})


    def send_user_agreement(self, usename, useid):
        to_address = self.help_email
        if not to_address:
            raise Exception("HELP EMAIL NOT CONFIGURED")
        user_id =useid
        message = '' + \
                  "\n\nUser Info:\n" + \
                  "Name: {}: {}\nID:{} \n".format(usename, user_id ) + \
                  "User submitted the contract \n" + \
                  "\n"
        subject = "Encore User Agrrement ({})".format(user_id)
        self.send_feedback_mail(to_address, subject, message, {"reply-to": ''})

    def send_failed_job(self, job_id="11111111-1111"):
        to_address = self.help_email 
        subject = "Encore Failed Job ({})".format(job_id[:8])
        message = "The following job has failed:\n\n{}".format(job_id)
        try:
            message += "\n\n" + url_for("user.get_job", job_id=job_id)
            job = Job.get(job_id, current_app.config)
            message += "\n\nName:{} ".format(job.name)
            owner = job.get_owner()
            message += "\n\nUser:{} ".format(owner.email)
        except:
            pass
        self.send_mail(to_address, subject, message)

    def send_mail(self, to_address, subject, body, headers=None):
        if headers is None:
            headers = {}
        if not to_address:
            raise Exception("HELP EMAIL NOT CONFIGURED") 
        if not body:
            raise Exception("EMPTY MESSAGE") 
        from_address = headers.get("from", self.from_email)
        message = MIMEText(body)
        message["subject"] = subject 
        message["from"] = from_address
        message["to"] = to_address
        if "reply-to" in headers:
            message.add_header('reply-to', headers['reply-to'])
        smtp = smtplib.SMTP()
        smtp.connect(self.smtp_server)
        try:
            smtp.sendmail(from_address, to_address, message.as_string())
            return True
        except Exception as e:
            raise
        finally:
            smtp.quit()
        return False


    def send_feedback_mail(self, to_address, subject, body, headers=None):
        if headers is None:
            headers = {}
        if not to_address:
            raise Exception("HELP EMAIL NOT CONFIGURED")
        if not body:
            raise Exception("EMPTY MESSAGE")
        from_address = headers.get("from", self.from_email)
        message = MIMEText(body)
        message["subject"] = subject
        message["from"] = from_address
        message["to"] = to_address
        if "reply-to" in headers:
            message.add_header('reply-to', headers['reply-to'])
        smtp = smtplib.SMTP()
        smtp.connect(self.smtp_server)
        try:
            smtp.sendmail(from_address, to_address, message.as_string())
            return True
        except Exception as e:
            raise
        finally:
            smtp.quit()
        return False

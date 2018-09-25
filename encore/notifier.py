import smtplib
from email.mime.text import MIMEText

class Notifier:
    @staticmethod
    def send_user_feedback(user_email, user_fullname, user_message, from_page, user, config):
        to_address = config.get("HELP_EMAIL", None)
        from_address = "do-not-reply@encore.sph.umich.edu"
        if not to_address:
            raise ApiException("HELP EMAIL NOT CONFIGURED") 
        user_id = user.rid
        if not user_message:
            raise Exception("EMPTY MESSAGE") 
        message = MIMEText(user_message + \
            "\n\nUser Info:\n" + \
            "Name: {}\nEmail: {}\nID:{}".format(user_fullname, user_email, user_id) +  \
            "\n\nReferring Page:\n" + \
            from_page + \
            "\n")
        message["subject"] = "Encore User Feedback ({})".format(user_fullname)
        message["from"] = from_address
        message["to"] = to_address
        message.add_header('reply-to', user_email)
        smtp = smtplib.SMTP()
        smtp.connect(config.get("SMTP_SERVER", 'localhost'))
        try:
            smtp.sendmail(from_address, to_address, message.as_string())
            return True
        except Exception as e:
            raise
        finally:
            smtp.quit()
        return False

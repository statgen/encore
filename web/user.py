import MySQLdb
from flask import session, current_app
from flask_login import UserMixin


class User(UserMixin):

    def __init__(self, email, rid):
        self.email = email
        self.rid = rid

    def get_id(self):
        return self.email

    def can_view_job(self, job_id, db):
        if self.is_admin():
            return True
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute("SELECT user_id FROM jobs WHERE user_id=%s and job_id=%s", (self.rid, job_id,))
        if cur.rowcount > 0:
            return True
        return False

    def is_admin(self):
        return self.email in current_app.config.get("ADMIN_USERS",[]) 

    @staticmethod
    def from_email(email, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute("SELECT id, email FROM users WHERE email=%s", (email,))
        if cur.rowcount == 0:
            return None
        res = cur.fetchone()
        return User(res["email"], res["id"])


    @staticmethod
    def from_session_key(sess_key, db):
        if sess_key not in session:
            return None
        return User.from_email(session[sess_key], db)

import MySQLdb
from flask import session


class User(object):
    def __init__(self, email, rid=None):
        self.email = email
        self.rid = rid


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

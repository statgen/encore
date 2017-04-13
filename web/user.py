import MySQLdb
import sql_pool
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

    def log_login(self, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "UPDATE users SET last_login_date = NOW() WHERE id = %s"
        cur.execute(sql, (self.rid, ))
        db.commit()

    @staticmethod
    def from_email(email, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute("SELECT id, email FROM users WHERE email=%s", (email,))
        if cur.rowcount == 0:
            return None
        res = cur.fetchone()
        return User(res["email"], res["id"])
    
    @staticmethod
    def create(email, can_analyze, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "INSERT INTO users (email, can_analyze) values (%s, %s)"
        cur.execute(sql, (email, can_analyze))
        new_id = cur.lastrowid
        cur.execute("SELECT id, email FROM users WHERE id=%s", (new_id,))
        res = cur.fetchone()
        return User(res["email"], res["id"])

    @staticmethod
    def from_session_key(sess_key, db):
        if sess_key not in session:
            return None
        return User.from_email(session[sess_key], db)

    @staticmethod
    def list_all(config=None):
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT id, email, full_name, affiliation, DATE_FORMAT(creation_date, '%Y-%m-%d %H:%i:%s') AS creation_date, DATE_FORMAT(last_login_date, '%Y-%m-%d %H:%i:%s') AS last_login_date
            FROM users
            ORDER BY id
            """
        cur.execute(sql)
        results = cur.fetchall()
        return results

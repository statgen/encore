import MySQLdb
import sql_pool
from flask import session, current_app
from flask_login import UserMixin


class User(UserMixin):
    __dbfields = ["id", "email", "can_analyze", "full_name", "affiliation"]

    def __init__(self, email, rid, can_analyze, full_name, affiliation):
        self.email = email
        self.rid = rid
        self.can_analyze = can_analyze
        self.full_name = full_name
        self.affiliation = affiliation

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
        return User.__get_by_sql_where(db, "email=%s", (email,))

    @staticmethod
    def from_id(rid, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        return User.__get_by_sql_where(db, "id=%s", (rid,))
    

    @staticmethod
    def from_session_key(sess_key, db):
        if sess_key not in session:
            return None
        return User.from_email(session[sess_key], db)

    @staticmethod
    def __get_by_sql_where(db, where="", vals=()):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT id, email, can_analyze, full_name, affiliation
            FROM users"""
        if where:
            sql += " WHERE " + where
        cur.execute(sql, vals)
        if cur.rowcount != 1:
            return None
        res = cur.fetchone()
        return User(res["email"], res["id"], res["can_analyze"], res["full_name"], res["affiliation"])


    @staticmethod
    def create(new_values, db=None):
        updateable_fields = [x for x in User.__dbfields if x != "id"]
        fields = new_values.keys() 
        values = new_values.values()
        bad_fields = [x for x in fields if x not in updateable_fields]
        if db is None:
            db = sql_pool.get_conn()
        try:
            if len(bad_fields)>0:
                raise Exception("Invalid update field: {}".format(", ".join(bad_fields)))
            sql = "INSERT INTO users (" + \
                ", ".join(fields)+ \
                ") values (" + \
                ", ".join(["%s"] * len(values)) + \
                ")"
            cur = db.cursor(MySQLdb.cursors.DictCursor)
            cur.execute(sql, values)
            db.commit()
            new_id = cur.lastrowid
            new_user = User.__get_by_sql_where(db, "id=%s", (new_id,))
            result = {"created": True, "user": new_user}
        except Exception as e:
            result = {"created": False, "error": str(e)}
        return result

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

    def as_object(self):
        obj = {key: getattr(self, key) for key in self.__dbfields if hasattr(self, key)} 
        obj["id"] = self.rid
        return obj

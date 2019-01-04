import MySQLdb
from . import sql_pool
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
    def from_id(rid, db = None):
        if db is None:
            db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        return User.__get_by_sql_where(db, "id=%s", (rid,))
    

    @staticmethod
    def from_session_key(sess_key, db=None):
        if sess_key not in session:
            return None
        if db is None:
            db = sql_pool.get_conn()
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
        if "can_analyze" in new_values:
            new_values["can_analyze"] = new_values["can_analyze"]=="true"
        updateable_fields = [x for x in User.__dbfields if x != "id"]
        fields = list(new_values.keys()) 
        values = list(new_values.values())
        bad_fields = [x for x in fields if x not in updateable_fields]
        if db is None:
            db = sql_pool.get_conn()
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
        result = {"user": new_user}
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

    @staticmethod
    def counts(by=None, filters=None, config=None):
        join_jobs = False
        count = "COUNT(DISTINCT users.id)"
        if not by:
            by = []
        elif isinstance(by, str):
            by = by.split(",")
        if not filters:
            filters = []
        elif isinstance(filters, str):
            filters = filters.split(",")
        select = []
        group_by = []
        columns = []
        wheres = []
        for field in by:
            if field=="creation-month":
                select += [ "DATE_FORMAT(users.creation_date, '%Y-%m') as month"]
                group_by += [ "DATE_FORMAT(users.creation_date, '%Y-%m')"]
                columns += ["month"]
            elif field == "creation-year":
                select += ["year(users.creation_date) as year"]
                group_by += ["year(users.creation_date)"]
                columns += ["year"]
            elif field == "job-month":
                select += [ "DATE_FORMAT(jobs.creation_date, '%Y-%m') as month"]
                group_by += [ "DATE_FORMAT(jobs.creation_date, '%Y-%m')"]
                columns += ["month"]
                join_jobs = True
            elif field == "job-year":
                select += ["year(jobs.creation_date) as year"]
                group_by += ["year(jobs.creation_date)"]
                columns += ["year"]
                join_jobs = True
            else:
                raise Exception("Unrecognized field: {}".format(field))
        for filt in filters:
            if filt == "can-analyze":
                wheres += ["users.can_analyze = 1"]
            elif filt == "active":
                wheres += ["users.is_active = 1"]
            elif filt == "has-logged-in":
                wheres += ["users.last_login_date is not null"]
            elif filt == "successful":
                wheres += ["jobs.status_id = (select id from statuses where name = 'succeeded')"]
                join_jobs = True
            else:
                raise Exception("Unrecognized filter: {}".format(filt))
        select += [count + " as count"]
        columns += ["count"]
        sql = "SELECT " + ", ".join(select)
        sql += " FROM users"
        if join_jobs:
            sql += " JOIN jobs on users.id=jobs.user_id"
        if len(wheres):
            sql += " WHERE (" + "), (".join(wheres) + ")"
        if len(group_by):
            sql += " GROUP BY " + ", ".join(group_by)

        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql)
        results = cur.fetchall()
        return {"header": {"columns": columns}, "data": results}

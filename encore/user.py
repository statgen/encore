import MySQLdb
import re
from . import sql_pool
from collections import OrderedDict
from flask import session, current_app
from flask_login import UserMixin
from .db_helpers import SelectQuery, PagedResult, OrderClause, OrderExpression, WhereExpression, WhereAll, TableJoin


class User(UserMixin):
    __dbfields = ["id", "email", "can_analyze", "full_name","unique_name", "affiliation", "is_active"]

    def __init__(self, email, rid, can_analyze, full_name,unique_name, affiliation, is_active):
        self.email = email
        self.rid = rid
        self.can_analyze = can_analyze
        self.full_name = full_name
        self.unique_name = unique_name
        self.affiliation = affiliation
        self._is_active = is_active

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

    def is_active(self):
        return self._is_active

    def log_login(self, db):
        #print("in log login")
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "UPDATE users SET last_login_date = NOW() WHERE id = %s"
        cur.execute(sql, (self.rid, ))
        db.commit()

    def set_inactive(self,db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "UPDATE users SET is_active = %s"
        cur.execute(sql, (self.rid, ))
        db.commit()

    def get_collaborator(self, rid, db=None):
        where = WhereAll(
            WhereExpression("users.id in (select user_id from job_users where job_id in " +
              "(select job_id from job_users where user_id=%s and role_id=1) and user_id != %s)", (self.rid, self.rid)),
            WhereExpression("users.id = %s", (rid ,))
        )
        return User.__get_by_sql_where(db, where)

    def get_collaborators(self, query, db=None):
        where = WhereAll(
            WhereExpression("job_users.job_id in (select job_id from job_users where user_id=%s and role_id=1)", (self.rid,)),
            WhereExpression("job_users.user_id != %s", (self.rid,))
        )
        cols = User.__default_cols()
        group_by = list(cols.keys())
        cols["count"] = "count(*)"
        qcols = User.__default_qcols()
        sqlcmd = User.__build_sql_command(cols, qcols, where, query)
        sqlcmd.set_group_by(group_by)
        sqlcmd.add_join(TableJoin("job_users", "job_users.user_id = users.id"))
        return User.__list_by_sql_command(db, sqlcmd)

    def get_collaborations(self, query, db=None):
        if db is None:
            db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cols = OrderedDict([("user_id", "job_users.user_id"),
            ("user_email", "users.email"),
            ("user_full_name", "users.full_name"),
            ("user_affiliation", "users.affiliation"),
            ("job_id", "bin_to_uuid(jobs.id)"),
            ("job_name", "jobs.name"),
            ("job_creation_date", "DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("creation_date", "DATE_FORMAT(users.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("role_id", "job_users.role_id")
        ])
        qcols = ["user_id", "user_email", "user_full_name", "user_affiliation",
            "job_id", "jon_name", "job_creation_date", "creation_date"]
        page, order_by, qsearch = SelectQuery.translate_query(query, cols, qcols)
        if not order_by:
            order_by = OrderClause(OrderExpression(cols["creation_date"], "DESC"))
        where = WhereAll(
            WhereExpression("job_users.job_id in (select job_id from job_users where user_id=%s)", (self.rid,)),
            WhereExpression("job_users.user_id != %s", (self.rid,))
        )
        sqlcmd = (SelectQuery()
            .set_cols([ "{} AS {}".format(v,k) for k,v in cols.items()])
            .set_table("job_users")
            .add_join(TableJoin("users", "job_users.user_id = users.id"))
            .add_join(TableJoin("jobs", "job_users.job_id = jobs.id"))
            .set_search(qsearch)
            .set_where(where)
            .set_order_by(order_by)
            .set_page(page))
        return PagedResult.execute_select(db, sqlcmd)

    @staticmethod
    def from_email(email, db):
        where = WhereExpression("email=%s", (email, ))
        return User.__get_by_sql_where(db, where)

    def from_full_name(fullname, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        where = WhereExpression("full_name=%s", (fullname, ))
        return User.__get_by_sql_where(db, where)

    def from_unique_name(fullname, db):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        where = WhereExpression("unique_name=%s", (fullname, ))
        return User.__get_by_sql_where(db, where)

    @staticmethod
    def from_id(rid, db = None):
        if db is None:
            db = sql_pool.get_conn()
        where = WhereExpression("id=%s", (rid, ))
        return User.__get_by_sql_where(db, where)

    @staticmethod
    def from_session_key(sess_key, db=None):
        if sess_key not in session:
            return None
        if db is None:
            db = sql_pool.get_conn()
        return User.from_email(session[sess_key], db)

    @staticmethod
    def list_all(config=None, query=None):
        db = sql_pool.get_conn()
        return User.__list_by_sql_where_query(db, query=query)

    @staticmethod
    def __get_by_sql_where(db, where=None):
        sqlcmd = User.__build_default_sql_command(where)
        return User.__get_by_sql_command(db, sqlcmd)

    @staticmethod
    def __get_by_sql_command(db, sqlcmd):
        if db is None:
            db = sql_pool.get_conn()
        results = User.__list_by_sql_command(db, sqlcmd=sqlcmd).results
        if len(results)!=1:
            return None
        res = results[0]
        return User(res["email"], res["id"], res["can_analyze"],
            res["full_name"],res["unique_name"
                                 ""], res["affiliation"], res["is_active"])

    @staticmethod
    def __default_cols():
        return OrderedDict([("id", "users.id"),
            ("email", "users.email"),
            ("full_name", "users.full_name"),
            ("unique_name", "users.unique_name"),
            ("affiliation", "users.affiliation"),
            ("can_analyze", "users.can_analyze"),
            ("creation_date", "DATE_FORMAT(users.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("last_login_date", "DATE_FORMAT(users.last_login_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("is_active", "users.is_active")
        ])

    def __default_qcols():
        return ["id", "email", "full_name","unique_name", "affiliation", "creation_date", "last_login_date"]

    @staticmethod
    def __build_default_sql_command(where=None, query=None):
        return User.__build_sql_command(
            User.__default_cols(),
            User.__default_qcols(),
            where, query)

    @staticmethod
    def __build_sql_command(cols, qcols=None, where=None, query=None):
        page, order_by, qsearch = SelectQuery.translate_query(query, cols, qcols)
        if not order_by:
            order_by = OrderClause(OrderExpression(cols["creation_date"], "DESC"))
        sqlcmd = (SelectQuery()
            .set_cols([ "{} AS {}".format(v,k) for k,v in cols.items()])
            .set_table("users")
            .set_where(where)
            .set_search(qsearch)
            .set_order_by(order_by)
            .set_page(page))
        return sqlcmd

    @staticmethod
    def __list_by_sql_where(db, where="", vals=()):
        result = User.__list_by_sql_where_query(db, where=WhereExpression(where, vals), query=None)
        return result.results

    @staticmethod
    def __list_by_sql_where_query(db, where=None, query=None):
        sqlcmd = User.__build_default_sql_command(where, query)
        return User.__list_by_sql_command(db, sqlcmd)

    @staticmethod
    def __list_by_sql_command(db, sqlcmd):
        if db is None:
            db = sql_pool.get_conn()
        return PagedResult.execute_select(db, sqlcmd)

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
        new_user = User.from_id(new_id, db=db)
        result = {"user": new_user}
        return result

    @staticmethod
    def createUser(values,db=None):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute("""
            INSERT INTO users ( email,can_analyze,full_name,unique_name,affiliation,is_active)
            VALUES (%s, %s, %s, %s, %s, %s)
                       """, (values['email'], values["can_analyze"], values["fullname"], values["uniquename"],values["affiliation"], values["is_active"]))
        #print("above commit")
        db.commit()
        new_id = cur.lastrowid
        new_user = User.from_id(new_id, db=db)
        return new_user


    def as_object(self):
        obj = {key: getattr(self, key) for key in self.__dbfields if hasattr(self, key)} 
        obj["id"] = self.rid
        obj["is_active"] = self.is_active()
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

from datetime import datetime
from flask_login import current_user
import MySQLdb
from . import sql_pool

class AccessTracker():
    @staticmethod
    def LogJobAccess(job_id, user_id = None, access_date = None):
        if user_id is None:
            user_id = current_user.rid
        if access_date is None:
            access_date = datetime.now().strftime('%Y-%m-%d')
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "INSERT INTO access_job_log " \
            "SET user_id = %s, " \
            "job_id = uuid_to_bin(%s),"  \
            "access_date = %s " \
            "ON DUPLICATE KEY UPDATE count=count+1;"
        params = (user_id, job_id, access_date)
        try:
            cur.execute(sql, params)
            db.commit()
        except (MySQLdb.Error, MySQLdb.Warning) as e:
            print("Logging error:  {}".format(e))

    def LogAPIAccess(user_id = None, access_date = None):
        if user_id is None:
            user_id = current_user.rid
        if access_date is None:
            access_date = datetime.now().strftime('%Y-%m-%d')
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "INSERT INTO access_api_log " \
            "SET user_id = %s, " \
            "access_date = %s " \
            "ON DUPLICATE KEY UPDATE count=count+1;"
        params = (user_id, access_date)
        try:
            cur.execute(sql, params)
            db.commit()
        except (MySQLdb.Error, MySQLdb.Warning) as e:
            print("Logging error:  {}".format(e))

    @staticmethod
    def counts(what=None, by=None, filters=None, config=None):
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
        table_from = "FROM access_job_log as access"
        join_users = True
        for field in by:
            if field=="day":
                select += [ "DATE_FORMAT(access.access_date, '%Y-%m-%d') as day"]
                group_by += [ "DATE_FORMAT(access.access_date, '%Y-%m-%d')"]
                columns += ["day"]
            elif field=="month":
                select += [ "DATE_FORMAT(access.access_date, '%Y-%m') as month"]
                group_by += [ "DATE_FORMAT(access.access_date, '%Y-%m')"]
                columns += ["month"]
            elif field == "year":
                select += ["year(access.access_date) as year"]
                group_by += ["year(access.access_date)"]
                columns += ["year"]
            elif field == "user":
                select += ["COALESCE(users.full_name, users.email) as user"]
                group_by += ["users.id"]
                columns += ["user"]
                join_users = True
            else:
                raise Exception("Unrecognized field: {}".format(field))
        for filt in filters:
            raise Exception("Unrecognized filter: {}".format(filt))
        if what=="jobs":
            select += ["COUNT(DISTINCT user_id) as count"]
        elif what=="users":
            select += ["COUNT(DISTINCT job_id) as count"]
        elif what=="api":
            select += ["CAST(SUM(count) as SIGNED) as count"]
            table_from = "FROM access_api_log as access"
        else:
            raise Exception("Unrecognized what: {}".format(what))
        columns += ["count"]
        sql = "SELECT " + ", ".join(select)
        sql += " " + table_from
        if join_users:
            sql += " JOIN users on access.user_id = users.id"
        if len(wheres):
            sql += " WHERE (" + "), (".join(wheres) + ")"
        if len(group_by):
            sql += " GROUP BY " + ", ".join(group_by)

        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql)
        results = cur.fetchall()
        return {"header": {"columns": columns}, "data": results}

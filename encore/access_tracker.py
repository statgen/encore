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
        sql = "INSERT IGNORE INTO access_job_log " \
            "SET user_id = %s, " \
            "job_id = uuid_to_bin(%s),"  \
            "access_date = %s"
        params = (user_id, job_id, access_date)
        try:
            cur.execute(sql, params)
            db.commit()
        except (MySQLdb.Error, MySQLdb.Warning) as e:
            print("Logging error:  {}".format(e))

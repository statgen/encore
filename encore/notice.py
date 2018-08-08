import sql_pool
import MySQLdb

class Notice:
    def __init__(self, notice_id):
        self.notice_id = notice
        self.message = None
        self.start_date = None
        self.end_date = None

    def as_object(self):
        obj = {"notice_id": self.notice_id,
            "message": self.messsage, 
            "start_date": self.start_date,
            "end_date": self.end_date}
        return obj

    @staticmethod
    def get(notice_id, config):
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT id, mesaage
            DATE_FORMAT(start_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date 
            FROM notices
            WHERE id = uuid_to_bin(%s)
            """
        cur.execute(sql, (notice_id,))
        result = cur.fetchone()
        if result is not None:
            n = Notice(geno_id, meta)
            n.message = result["message"]
            n.start_date = result["start_date"]
            n.end_date = result["end_date"]
        else:
            n = None
        return n
        
    @staticmethod
    def list_current(config = None):
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT id, message, 
            DATE_FORMAT(start_date, '%Y-%m-%d %H:%i:%s') AS creation_date,
            DATE_FORMAT(end_date, '%Y-%m-%d %H:%i:%s') AS end_date
            FROM notices 
            WHERE start_date <= CURRENT_TIMESTAMP() and (end_date is NULL or end_date > CURRENT_TIMESTAMP())
            ORDER BY start_date DESC
            """
        cur.execute(sql)
        results = cur.fetchall()
        return results

from . import sql_pool
from .db_helpers import SelectQuery, TableJoin, PagedResult, OrderClause, OrderExpression, WhereExpression, WhereAll
from collections import OrderedDict
import MySQLdb

class Notice:
    __dbfields = ["id", "message", "start_date", "end_date"]

    def __init__(self, notice_id, message=None, start_date=None, end_date=None):
        self.notice_id = notice_id
        self.message = message
        self.start_date = start_date
        self.end_date = end_date

    def as_object(self):
        obj = {"notice_id": self.notice_id,
            "message": self.message,
            "start_date": self.start_date,
            "end_date": self.end_date}
        return obj

    @staticmethod
    def get(notice_id, db=None):
        if db is None:
            db = sql_pool.get_conn()
        where = WhereExpression("id=%s", (notice_id, ))
        return Notice.__get_by_sql_where(db, where)

        
    @staticmethod
    def list_current(config = None, query=None):
        db = sql_pool.get_conn()
        params = (query and query.params) or dict()
        params["is_active"] = True
        where, joins = Notice.__params_to_where(params)
        result = Notice.__list_by_sql_where_query(db, where=where, query=query)
        return result

    @staticmethod
    def list_all(config=None, query=None):
        db = sql_pool.get_conn()
        where, joins = Notice.__params_to_where(query.params)
        result = Notice.__list_by_sql_where_query(db, where=where, query=query)
        return result

    @staticmethod
    def __get_by_sql_where(db, where=None):
        if db is None:
            db = sql_pool.get_conn()
        results = Notice.__list_by_sql_where_query(db, where, None).results
        if len(results)!=1:
            return None
        res = results[0]
        return Notice(res["id"], res["message"],
            res["start_date"], res["end_date"])

    @staticmethod
    def __list_by_sql_where_query(db, where=None, query=None):
        cols = OrderedDict([("id", "id"),
            ("message", "message"),
            ("start_date", "DATE_FORMAT(start_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("end_date", "DATE_FORMAT(end_date, '%%Y-%%m-%%d %%H:%%i:%%s')")
        ])
        qcols = ["id", "message", "start_date", "end_date"]
        page, order_by, qsearch = SelectQuery.translate_query(query, cols, qcols)
        if not order_by:
            order_by = OrderClause(OrderExpression(cols["start_date"], "DESC"))
        sqlcmd = (SelectQuery()
            .set_cols([ "{} AS {}".format(v,k) for k,v in cols.items()])
            .set_table("notices")
            .set_where(where)
            .set_search(qsearch)
            .set_order_by(order_by)
            .set_page(page))
        return PagedResult.execute_select(db, sqlcmd)

    @staticmethod
    def __params_to_where(params):
        where = WhereAll()
        joins = dict()
        for k, v in params.items():
            if k == "is_active":
                where.add(WhereExpression("start_date <= CURRENT_TIMESTAMP() and (end_date is NULL or end_date > CURRENT_TIMESTAMP())"))
            else:
                raise Exception("Parameter {} Not Recognized".format(k))
        return where, joins

    @staticmethod
    def create(new_values, db=None):
        updateable_fields = [x for x in Notice.__dbfields if x != "id"]
        fields = list(new_values.keys())
        values = [None if x=="" else x for x in new_values.values()]
        bad_fields = [x for x in fields if x not in updateable_fields]
        if db is None:
            db = sql_pool.get_conn()
        if len(bad_fields)>0:
            raise Exception("Invalid create field: {}".format(", ".join(bad_fields)))
        sql = "INSERT INTO notices (" + \
            ", ".join(fields)+ \
            ") values (" + \
            ", ".join(["%s"] * len(values)) + \
            ")"
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql, values)
        db.commit()
        new_id = cur.lastrowid
        new_notice = Notice.get(new_id, None)
        result = {"notice": new_notice}
        return result

    @staticmethod
    def update(notice_id, new_values):
        updateable_fields = [x for x in Notice.__dbfields if x != "id"]
        fields = list(new_values.keys())
        values = [None if x=="" else x for x in new_values.values()]
        bad_fields = [x for x in fields if x not in updateable_fields]
        if len(bad_fields)>0:
            raise Exception("Invalid update field: {}".format(", ".join(bad_fields)))
        sql = "UPDATE notices SET "+ \
            ", ".join(("{}=%s".format(k) for k in fields)) + \
            " WHERE id = %s"
        db = sql_pool.get_conn()
        cur = db.cursor()
        cur.execute(sql, values + [int(notice_id)])
        db.commit()

import re
import MySQLdb
from collections import namedtuple
from math import ceil

class TableJoin:
    def __init__(self, table, on, join_type="LEFT"):
        self.table = table
        self.on = on
        self.join_type = "LEFT"

    def to_clause(self):
        return "{} JOIN {} ON {}".format(self.join_type, self.table, self.on)

class SelectQuery:
    def __init__(self):
        self.cols = []
        self.table = ""
        self.joins = []
        self.where = ""
        self.vals = ()
        self.order = ""
        self.page = None

    @staticmethod
    def __base_sql(cols=[], table="", joins=[], where="", vals=(), order="", page=None):
        sql = "SELECT "
        sql += ", ".join(cols)
        sql += " FROM " + table
        for join in joins:
            sql += " " + join.to_clause()
        if where:
            sql += " WHERE " + where
        if order:
            sql += " ORDER BY " + order
        if page:
            sql += " LIMIT %s OFFSET %s"
            vals += (page.limit, page.offset)
        return sql, vals

    def cmd_select(self):
        sql, vals = SelectQuery.__base_sql(self.cols, self.table, self.joins,
            self.where, self.vals, self.order, self.page)
        return sql, vals

    def cmd_count(self):
        sql, vals = SelectQuery.__base_sql(["count(*) as count"], self.table, self.joins,
            self.where, self.vals)
        return sql, vals

    def set_cols(self, cols):
        self.cols = cols
        return self

    def add_col(self, col):
        self.cols.append(col)
        return self

    def set_table(self, table):
        self.table = table
        return self

    def set_joins(self, joins):
        self.joins = joins
        return self

    def add_join(self, join):
        self.joins.append(join)
        return self

    def set_where(self, where):
        self.where = where
        return self

    def set_vals(self, vals):
        self.vals = vals
        return self

    def set_order(self, order):
        self.order = order
        return self

    def set_page(self, page):
        self.page = page
        return self


PageInfo = namedtuple('PageInfo', ['limit', 'offset'], verbose=False)

class PagedResult:
    def __init__(self, results, total_count=0, page=None):
        self.results = results
        self.page = page
        self.total_count = total_count

    def next_page(self):
        if self.page is None:
            return None
        if self.page.offset + self.page.limit >= self.total_count:
            return None
        return PageInfo(self.page.limit, self.page.offset + self.page.limit)

    def prev_page(self):
        if self.page is None:
            return None
        if self.page.offset == 0:
            return None
        return PageInfo(self.page.limit, min(self.page.offset-self.page.limit, 0))

    def page_count(self):
        if self.page is None:
            if self.total_count>0:
                return 1
            else:
                return 0
        return int(ceil(self.total_count / float(self.page.limit)))

    @staticmethod
    def execute_select(db, sqlcmd):
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql, vals =  sqlcmd.cmd_select()
        page = sqlcmd.page
        cur.execute(sql, vals)
        results = cur.fetchall()
        if page and (page.offset>0 or len(results)==page.limit):
            sql, vals = sqlcmd.cmd_count()
            cur.execute(sql, vals)
            total_count = cur.fetchone()["count"]
        else:
            total_count = len(results)
        return PagedResult(results, total_count, page)

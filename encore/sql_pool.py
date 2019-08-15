# Not actually implementing a pool at this point.

import MySQLdb
from flask import g, current_app

def register_db(app):
    @app.teardown_appcontext
    def teardown_conn(exception):
        con = getattr(g, '_database_con', None)
        if con is not None:
            con.close()

def get_conn():
    con = getattr(g, '_database_con', None)
    if con is None:
        con = g._database_con = MySQLdb.connect(
        host=current_app.config.get("MYSQL_HOST"), 
        user=current_app.config.get("MYSQL_USER"), 
        passwd=current_app.config.get("MYSQL_PASSWORD"), 
        db=current_app.config.get("MYSQL_DB"))
    return con


# Not actually implementing a pool at this point. I don't know enough about Flask contexts to safely implement connection pooling.

import MySQLdb
from flask import current_app

def get_conn():
    return MySQLdb.connect(host=current_app.config.get("MYSQL_HOST"), user=current_app.config.get("MYSQL_USER"), passwd=current_app.config.get("MYSQL_PASSWORD"), db=current_app.config.get("MYSQL_DB"))
import MySQLdb
from threading import Timer
from datetime import datetime
import sys

class Job(object):
    def __init__(self, rid, status):
        self.id = rid
        self.status = status

class DatabaseCredentials(object):
    def __init__(self, host, user, pw, db):
        self.host = host
        self.user = user
        self.pw = pw
        self.db = db

class Tracker(object):

    def __init__(self, interval, db_credentials):
        self.interval = interval
        self.credentials = db_credentials
        self.timer = None

    @staticmethod
    def query_pending_jobs(db):

        sql = ("SELECT bin_to_uuid(jobs.id) AS id, statuses.name AS status FROM jobs "
               "LEFT JOIN statuses ON statuses.id = jobs.status_id "
               "WHERE (statuses.name='created' OR statuses.name='queued' OR statuses.name='started' OR statuses.name='cancel_requested')")

        cur = db.cursor(MySQLdb.cursors.DictCursor)
        jobs = []
        for x in xrange(0, cur.rowcount):
            row = cur.fetchone()
            jobs.append(Job(row["id"], row["status"]))

        return jobs

    @staticmethod
    def check_for_job_status_update(db, job):
        return True

    def routine(self):
        db = MySQLdb.connect(host=self.credentials.host, user=self.credentials.user, passwd=self.credentials.pw, db=self.credentials.db)
        jobs = Tracker.query_pending_jobs(db)
        for j in jobs:
            Tracker.check_for_job_status_update(db, j)

    def timer_callback(self):
        sys.stderr.write(str(datetime.now()) + "\n")
        self.routine()
        self.start()

    def cancel(self):
        self.timer.cancel()

    def start(self):
        self.timer = Timer(self.interval, self.timer_callback)
        self.timer.daemon = True
        self.timer.start()
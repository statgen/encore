import MySQLdb
from threading import Timer
from datetime import datetime
import sys
import subprocess
import os
import datetime
import pwd

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
               "WHERE (statuses.name='queued' OR statuses.name='started')")

        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql)
        jobs = []
        for x in xrange(0, cur.rowcount):
            row = cur.fetchone()
            jobs.append(Job(row["id"], row["status"]))
        return jobs

    @staticmethod
    def update_job_status(db, job, slurm_status, exit_code):
        status = ""
        reason = ""
        if slurm_status == "RUNNING":
            status = "started"
        elif slurm_status == "CANCELLED by 0":
            status = "failed"
            reason = "Insufficient resource allocation"
        elif slurm_status.startswith("CANCELLED"):
            status = "cancelled"
        elif slurm_status == "PENDING" or slurm_status == "QUEUED":
            status = "queued"
        elif slurm_status == "TIMEOUT":
            status = "failed"
            reason = "Exceeded allocated time"
        elif slurm_status == "PREEMPTED" or slurm_status == "FAILED" or slurm_status == "NODE_FAIL":
            status = "failed"
        elif slurm_status == "COMPLETED":
            status = "succeeded"

        if status:
            cur = db.cursor(MySQLdb.cursors.DictCursor)
            if reason:
                sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name=%s LIMIT 1), " +  \
                    "error_message=%s, " + \
                    "modified_date = NOW() WHERE id = uuid_to_bin(%s)"
                cur.execute(sql, (status, reason, job.id))
            else:
                sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name=%s LIMIT 1), " + \
                    "modified_date = NOW() WHERE id = uuid_to_bin(%s)"
                cur.execute(sql, (status, job.id))
            db.commit()

    @staticmethod
    def update_job_statuses(db, jobs):
        # job_names_param = ",".join("gasp_" + x.id for x in jobs)
        p = subprocess.Popen(["/usr/cluster/bin/sacct", "-u", pwd.getpwuid(os.getuid())[0], "--format", "jobid,state,exitcode,jobname", "--noheader", "-P", "-S", (datetime.date.today() - datetime.timedelta(days=30)).strftime("%Y-%m-%d")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        squeue_out, squeue_err = p.communicate()

        fake_data = """29646434            PENDING             0
29646435            COMPLETED             0
"""

        # only keep last record for jobs that were re-run
        slurm_jobs_found = dict()
        for line in squeue_out.split("\n"):
            if line:
                slurm_job = line.strip().split("|")
                # strip off "gasp_"
                slurm_jobs_found[slurm_job[3][5:]] = slurm_job
        for slurm_job in slurm_jobs_found.values():
            for j in jobs:
                if slurm_job[3][5:] == j.id:
                    Tracker.update_job_status(db, j, slurm_job[1], slurm_job[2])
                    break

    def routine(self):
        db = MySQLdb.connect(host=self.credentials.host, user=self.credentials.user, passwd=self.credentials.pw, db=self.credentials.db)
        jobs = Tracker.query_pending_jobs(db)
        if len(jobs) != 0:
            Tracker.update_job_statuses(db, jobs)


    def timer_callback(self):
        try:
            self.routine()
        except:
            True
        self.start()

    def cancel(self):
        self.timer.cancel()

    def start(self):
        self.timer = Timer(self.interval, self.timer_callback)
        self.timer.daemon = True
        self.timer.start()

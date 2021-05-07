import MySQLdb
from threading import Timer
from datetime import datetime
import sys
import subprocess
import os
import datetime
import pwd
from flask import current_app
from .notifier import get_notifier
from .job import Job
from . import sql_pool

class JobRec(object):
    def __init__(self, rid, status):
        self.id = rid
        self.status = status

class Tracker(object):

    def __init__(self, interval, app):
        self.interval = interval
        self.app = app
        self.timer = None

    def query_pending_jobs(self, db):

        sql = ("SELECT bin_to_uuid(jobs.id) AS id, statuses.name AS status FROM jobs "
               "LEFT JOIN statuses ON statuses.id = jobs.status_id "
               "WHERE (statuses.name='queued' OR statuses.name='started' or statuses.name='canceling')")

        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql)
        jobs = []
        for x in range(0, cur.rowcount):
            row = cur.fetchone()
            jobs.append(JobRec(row["id"], row["status"]))
        return jobs

    def update_job_status(self, db, job_id, slurm_status, exit_code, config):
        status = ""
        reason = ""
        job = None
        try:
            job = Job.get(job_id, config)
        except Exception as e:
            print("Error Fetching Job in Tracker")
            print(e)
        if slurm_status == "RUNNING":
            status = "started"
        elif slurm_status == "CANCELLED by 0":
            status = "failed"
            reason = "Insufficient resource allocation"
        elif slurm_status.startswith("CANCELLED"):
            status = "canceled"
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
                cur.execute(sql, (status, reason, job_id))
            else:
                sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name=%s LIMIT 1), " + \
                    "modified_date = NOW() WHERE id = uuid_to_bin(%s)"
                cur.execute(sql, (status, job_id))
            db.commit()
            notifier = get_notifier()
            if notifier:
                try:
                    if status == "failed":
                        notifier.send_failed_job(job_id, job)
                except:
                    pass

    def update_job_statuses(self, db, jobs, config):
        p = subprocess.Popen(["/usr/cluster/bin/sacct", "-u", pwd.getpwuid(os.getuid())[0], \
            "--format", "jobid,state,exitcode,jobname,submit", "--noheader", "-P", \
            "-S", (datetime.date.today() - datetime.timedelta(days=30)).strftime("%Y-%m-%d")], \
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        squeue_out, squeue_err = p.communicate()

        # keep only most recent submission date for each job
        slurm_jobs_found = dict()
        for line in squeue_out.decode().rstrip().split("\n"):
            if line:
                slurm_job = line.strip().split("|")
                # strip off "gasp_"
                job_name = slurm_job[3][5:]
                if job_name in slurm_jobs_found:
                    prev_date = datetime.datetime.strptime(slurm_jobs_found[job_name][4], '%Y-%m-%dT%H:%M:%S')
                    curr_date = datetime.datetime.strptime(slurm_job[4], '%Y-%m-%dT%H:%M:%S')
                    if curr_date > prev_date:
                        slurm_jobs_found[job_name] = slurm_job
                else:
                    slurm_jobs_found[job_name] = slurm_job
        jobs_updated = 0
        for slurm_job in slurm_jobs_found.values():
            for j in jobs:
                if slurm_job[3][5:] == j.id:
                    self.update_job_status(db, j.id, slurm_job[1], slurm_job[2], config)
                    jobs_updated += 1
                    break
        return jobs_updated

    def routine(self):
        with self.app.app_context():
            db = sql_pool.get_conn()
            config = current_app.config
            try:
                jobs = self.query_pending_jobs(db)
                if len(jobs) != 0:
                    self.update_job_statuses(db, jobs, config)
            except Exception as e:
                print("Tracker Call Back Error")
                print(e)

    def timer_callback(self):
        try:
            self.routine()
        except:
            True
        self.start()

    def cancel(self):
        if self.timer:
            self.timer.cancel()

    def start(self):
        self.timer = Timer(self.interval, self.timer_callback)
        self.timer.daemon = True
        self.timer.start()

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

class JobRec(object):
    def __init__(self, rid, status):
        self.id = rid
        self.status = status

class Tracker(object):

    def __init__(self, interval, app):
        self.interval = interval
        self.app = app
        self.timer = None

    def query_pending_jobs(self):

        joblist = Job.list_all_active()

        jobs = []
        for row in joblist:
            jobs.append(JobRec(row["id"], row["status"]))
        return jobs

    def update_job_status(self, job_id, slurm_status, exit_code, old_status, config):
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
        elif slurm_status == "OUT_OF_MEMORY":
            status = "failed"
            reason = "Out of memory"
        elif slurm_status.startswith("CANCELLED"):
            status = "canceled"
        elif slurm_status == "PENDING" or slurm_status == "QUEUED":
            status = "queued"
        elif slurm_status == "TIMEOUT":
            status = "failed"
            reason = "Exceeded allocated time"
        elif slurm_status == "PREEMPTED" or slurm_status == "FAILED" or slurm_status == "NODE_FAIL":
            status = "failed"
            if job:
                reason = job.get_failure_reason() or ""
        elif slurm_status == "COMPLETED":
            status = "succeeded"

        if status:
            Job.update_status(job_id, status, reason, old_status)
            notifier = get_notifier()
            if notifier:
                try:
                    if status == "failed":
                        job = Job.get(job_id, config)
                        notifier.send_failed_job(job_id, job)
                except:
                    pass

    def update_job_statuses(self, jobs, config):
        p = subprocess.Popen(["sacct", "-u", pwd.getpwuid(os.getuid())[0], \
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
                    self.update_job_status(j.id, slurm_job[1], slurm_job[2], j.status, config)
                    jobs_updated += 1
                    break
        return jobs_updated

    def routine(self):
        with self.app.app_context():
            config = current_app.config
            try:
                jobs = self.query_pending_jobs()
                if len(jobs) != 0:
                    self.update_job_statuses(jobs, config)
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

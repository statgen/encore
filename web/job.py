import os
import json
import sql_pool
import MySQLdb

class Job:
    __dbfields = ["user_id","name","error_message","status_id","creation_date","modified_date"]
    __extfields = ["status", "role"]

    def __init__(self, job_id, meta=None):
        self.job_id = job_id
        map(lambda x: setattr(self, x, None), self.__dbfields + self.__extfields)
        self.root_path = "" 
        self.users = []
        self.root_folder = ""
        self.meta = meta

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.root_path, *args))

    def as_object(self):
        obj = {key: getattr(self, key) for key in self.__dbfields  + self.__extfields if hasattr(self, key)} 
        obj["job_id"] = self.job_id
        obj["users"] = self.users
        return obj

    @staticmethod
    def get(job_id, config):
        job_folder = os.path.join(config.get("JOB_DATA_FOLDER", "./"), job_id)
        meta_path = os.path.expanduser(os.path.join(job_folder, "meta.json"))
        if os.path.exists(meta_path):
            with open(meta_path) as meta_file:
                meta = json.load(meta_file)
        else:
           meta = dict()
        j = Job(job_id, meta)
        j.root_path = job_folder
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT
              bin_to_uuid(jobs.id) AS id,
              jobs.name AS name, jobs.user_id as user_id,
              jobs.status_id as status_id, statuses.name AS status,
              jobs.error_message AS error_message,
              DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date,
              DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
            FROM jobs
            LEFT JOIN statuses ON jobs.status_id = statuses.id
            WHERE jobs.id = uuid_to_bin(%s)
            """
        cur.execute(sql, (job_id,))
        result = cur.fetchone()
        map(lambda x: setattr(j, x, result[x]), \
            (val for val in Job.__dbfields + Job.__extfields if val in result))
        sql = """
            SELECT 
                ju.user_id as user_id, ju.role_id as role_id,
                role.role_name as role, users.email, 
                DATE_FORMAT(ju.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
            FROM job_users as ju
                LEFT JOIN job_user_roles role on role.id = ju.role_id
                LEFT JOIN users on users.id = ju.user_id
            WHERE 
                ju.job_id = uuid_to_bin(%s)
            """
        cur.execute(sql, (job_id,))
        result = cur.fetchall()
        j.users = result
        return j

    @staticmethod
    def list_all_for_user(user_id, config=None):
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT
              bin_to_uuid(jobs.id) AS id,
              jobs.name AS name, ju.user_id as user_id,
              ju.role_id as role_id, role.role_name as role,
              jobs.user_id as owner_id, users.email as owner,
              jobs.status_id as status_id, statuses.name AS status,
              jobs.error_message AS error_message,
              DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date,
              DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
            FROM job_users as ju
                LEFT JOIN job_user_roles role on role.id = ju.role_id
                LEFT JOIN jobs on jobs.id = ju.job_id
                LEFT JOIN users on users.id = jobs.user_id
            LEFT JOIN statuses ON jobs.status_id = statuses.id
            WHERE ju.user_id = %s
            """
        cur.execute(sql, (user_id,))
        results = cur.fetchall()
        return results 

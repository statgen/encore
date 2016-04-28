import os
from flask import render_template, request, json, Response, current_app, session, redirect
import sql_pool
import MySQLdb
import uuid
import subprocess
#from werkzeug import secure_filename


ALLOWED_EXTENSIONS = set(['ped'])


# def allowed_file(filename):
#     return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS


def post_to_jobs():
    resp = Response(mimetype='application/json')
    if "user_id" not in session:
        resp.status_code = 400
        resp.set_data(json.dumps({"error": "Session expired."}))
    else:
        if request.method != 'POST':
            resp.status_code = 405
            resp.set_data(json.dumps({"error": "Not a POST request."}))
        else:
            if "job_name" not in request.form or "ped_file" not in request.files:
                resp.status_code = 400
                resp.set_data(json.dumps({"error": "Invalid file."}))
            else:
                job_id = str(uuid.uuid4())
                ped_file = request.files["ped_file"];
                job_directory = os.path.join(current_app.config.get("UPLOAD_FOLDER", "./"), job_id)
                os.mkdir(job_directory)
                ped_file_path = os.path.join(job_directory, "input.ped")
                ped_file.save(ped_file_path)

                validate_exe = os.path.join(current_app.config.root_path, "../bin/validate-ped")
                p = subprocess.Popen([validate_exe, ped_file_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                out, err = p.communicate()
                if p.returncode != 0:
                    if job_id: # rmtree would be bad if uuid4() returns an empty string.
                        os.rmtree(job_directory)
                    resp.status_code = 400
                    resp.set_data(json.dumps({"error": err}))
                else:
                    db = sql_pool.get_conn()
                    db.begin()
                    try:
                        cur = db.cursor()
                        cur.execute("INSERT INTO jobs (id, name, user_id) VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s)", (job_id, request.form["job_name"], session["user_id"]))
                        sql = """
                            INSERT INTO status_changes (job_id, status_id)
                            SELECT uuid_to_bin(%s), statuses.id
                            FROM statuses
                            WHERE statuses.name = 'created'
                            LIMIT 1
                            """
                        cur.execute(sql, (job_id,))
                        db.commit()
                    except:
                        db.rollback()
                        raise

                    #TODO: Queue Job
                    resp.set_data(json.dumps({"id": job_id}))
    return resp

def get_jobs():
    resp = Response(mimetype='application/json')

    if "user_id" not in session:
        resp.status_code = 400;
        resp.set_data(json.dumps({"error":"Session expired"}))
    else:
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(earliest_statuses.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date
            FROM jobs
            LEFT JOIN
            (
                SELECT job_id, MIN(creation_date) AS creation_date
                FROM status_changes
                GROUP BY job_id
            ) AS earliest_statuses
            ON earliest_statuses.job_id = jobs.id
            LEFT JOIN
            (
                SELECT job_id, MAX(id) AS id
                FROM status_changes
                GROUP BY job_id
            ) AS latest_statuses
            ON latest_statuses.job_id = jobs.id
            LEFT JOIN status_changes AS current_statuses ON current_statuses.id = latest_statuses.id
            LEFT JOIN statuses ON current_statuses.status_id = statuses.id
            WHERE jobs.user_id = %s
            ORDER BY earliest_statuses.creation_date DESC
            """
        cur.execute(sql, (session["user_id"],))
        results = cur.fetchall()

        resp.set_data(json.dumps(results))

    return resp

def get_job(job_id):
    resp = Response(mimetype='application/json')
    return resp

def get_job_details_view(job_id):
    if "user_id" not in session:
        return redirect("/sign-in")
    else:
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(earliest_statuses.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date
            FROM jobs
            LEFT JOIN
            (
                SELECT job_id, MIN(creation_date) AS creation_date
                FROM status_changes
                GROUP BY job_id
            ) AS earliest_statuses
            ON earliest_statuses.job_id = jobs.id
            LEFT JOIN
            (
                SELECT job_id, MAX(id) AS id
                FROM status_changes
                GROUP BY job_id
            ) AS latest_statuses
            ON latest_statuses.job_id = jobs.id
            LEFT JOIN status_changes AS current_statuses ON current_statuses.id = latest_statuses.id
            LEFT JOIN statuses ON current_statuses.status_id = statuses.id
            WHERE jobs.user_id = %s AND jobs.id = uuid_to_bin(%s)
            """
        cur.execute(sql, (session["user_id"], job_id))

        if cur.rowcount == 0:
            return "Job does not exist.", 404
        else:
            job_data = cur.fetchone()

            sql = """
                SELECT DATE_FORMAT(status_changes.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS ts, statuses.name as status
                FROM jobs
                LEFT JOIN status_changes ON jobs.id = status_changes.job_id
                LEFT JOIN statuses ON status_changes.status_id = statuses.id
                WHERE jobs.user_id = %s AND status_changes.job_id = uuid_to_bin(%s)
                ORDER BY status_changes.creation_date DESC, status_changes.id DESC
                """
            cur.execute(sql, (session["user_id"], job_id))
            job_data["history"] = cur.fetchall()

            return render_template("job_details.html", job=job_data)

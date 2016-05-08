import os
from flask import render_template, request, json, Response, current_app, redirect, send_file
from user import User
import sql_pool
import MySQLdb
import uuid
import subprocess
#from werkzeug import secure_filename


ALLOWED_EXTENSIONS = set(['ped'])


# def allowed_file(filename):
#     return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS

def get_job_list_view():
    user = User.from_session_key("user_email", sql_pool.get_conn())
    if not user:
        return redirect("/sign-in")
    return render_template("job_list.html")

def post_to_jobs():
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        resp.status_code = 401
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
                job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
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
                    cur = db.cursor()
                    cur.execute("""
                        INSERT INTO jobs (id, name, user_id, status_id)
                        VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s, (SELECT id FROM statuses WHERE name = 'created'))
                        """, (job_id, request.form["job_name"], user.rid))
                    db.commit()

                    resp.set_data(json.dumps({"id": job_id}))
    return resp

def get_jobs():
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        resp.status_code = 401;
        resp.set_data(json.dumps({"error":"Session expired"}))
    else:
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date, DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
            FROM jobs
            LEFT JOIN statuses ON jobs.status_id = statuses.id
            WHERE jobs.user_id = %s
            ORDER BY jobs.creation_date DESC
            """
        cur.execute(sql, (user.rid,))
        results = cur.fetchall()
        resp.set_data(json.dumps(results))
    return resp


def get_job(job_id):
    resp = Response(mimetype='application/json')
    return resp


def get_job_details_view(job_id):
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        return redirect("/sign-in")
    else:
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date, DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
            FROM jobs
            LEFT JOIN statuses ON jobs.status_id = statuses.id
            WHERE jobs.user_id = %s AND jobs.id = uuid_to_bin(%s)
            """
        cur.execute(sql, (user.rid, job_id))

        if cur.rowcount == 0:
            return "Job does not exist.", 404
        else:
            job_data = cur.fetchone()

            return render_template("job_details.html", job=job_data)

def get_job_output(job_id):
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        return redirect("/sign-in")
    else:
        try:
            job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
            output_file_path = os.path.join(job_directory, "output.epacts")
            return send_file(output_file_path, as_attachment=True)

        except:
            return "File Not Found", 404

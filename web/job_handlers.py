import os
import shutil
from flask import render_template, request, json, Response, current_app, redirect, send_file, make_response
from user import User
import sql_pool
import MySQLdb
import uuid
import subprocess
import tabix
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
    resp = Response(mimetype="application/json")
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        resp.status_code = 401
        resp.set_data(json.dumps({"error": "SESSION EXPIRED"}))
    else:
        if request.method != 'POST':
            resp.status_code = 405
            resp.mimetype = "text/plain"
            resp.set_data(json.dumps({"error": "NOT A POST REQUEST"}))
        else:
            if "job_name" not in request.form or "ped_file" not in request.files:
                resp.status_code = 400
                resp.set_data(json.dumps({"error": "INVALID FILE"}))
            else:
                job_id = str(uuid.uuid4())
                if not job_id:
                    resp.status_code = 500
                    resp.set_data(json.dumps({"error": "COULD NOT GENERATE JOB ID"}))
                else:
                    ped_file = request.files["ped_file"]
                    job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
                    os.mkdir(job_directory)
                    os.chmod(job_directory, 0o777)
                    ped_file_path = os.path.join(job_directory, "input.ped")
                    ped_file.save(ped_file_path)

                    validate_exe = os.path.join(current_app.config.root_path, "../bin/validate-ped")
                    p = subprocess.Popen([validate_exe, ped_file_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    out, err = p.communicate()
                    if p.returncode != 0:
                        shutil.rmtree(job_directory)
                        resp.status_code = 400
                        resp.set_data(json.dumps({"error": err.upper()}))
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
        resp.status = "SESSION EXPIRED"
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


def cancel_job(job_id):
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        return redirect("/sign-in")
    else:
        resp = Response(mimetype='application/json')
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name='cancel_requested' LIMIT 1) WHERE id = uuid_to_bin(%s) AND user_id = %s"
        cur.execute(sql, (job_id, user.rid))
        db.commit()
        if cur.rowcount == 0:
            resp.status_code = 404
            resp.status = "JOB NOT FOUND"
        return resp


def get_job_details_view(job_id):
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        return redirect("/sign-in")
    else:
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT
              bin_to_uuid(jobs.id) AS id,
              jobs.name AS name,
              statuses.name AS status,
              jobs.error_message AS error_message,
              DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date,
              DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
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


def get_job_output(job_id, filename, as_attach):
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not user:
        return redirect("/sign-in")
    else:
        #TODO: Check that user owns job id.
        try:
            job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
            output_file_path = os.path.join(job_directory, filename)
            return send_file(output_file_path, as_attachment=as_attach)
        except:
            return "File Not Found", 404


def get_job_zoom(job_id):
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    user = User.from_session_key("user_email", db)
    if not True: #user:
        resp.status_code = 401;
        resp.status = "SESSION EXPIRED"
    else:
        tb = tabix.open(os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id, "output.epacts.gz"))
        chrom = request.args.get("chrom", "")
        start_pos = int(request.args.get("start_pos", "0"))
        end_pos = int(request.args.get("end_pos", "0"))
        results = tb.query(chrom, start_pos, end_pos)
        json_response_data = dict()

        json_response_data["CHROM"] = []
        json_response_data["BEGIN"] = []
        json_response_data["END"] = []
        json_response_data["MARKER_ID"] = []
        #json_response_data["NS"] = []
        #json_response_data["AC"] = []
        #json_response_data["CALLRATE"] = []
        json_response_data["MAF"] = []
        json_response_data["PVALUE"] = []
        for r in results:
            json_response_data["CHROM"].append(r[0])
            json_response_data["BEGIN"].append(r[1])
            json_response_data["END"].append(r[2])
            json_response_data["MARKER_ID"].append(r[3])
            #json_response_data["NS"].append(r[4])
            #json_response_data["AC"].append(r[5])
            #json_response_data["CALLRATE"].append(r[6])
            json_response_data["MAF"].append(r[7])
            json_response_data["PVALUE"].append(r[8])
        resp.set_data(json.dumps(json_response_data))
    return resp
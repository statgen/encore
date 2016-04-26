import os
from flask import Flask, request, json, Response, current_app
import sql_pool
import uuid
#from werkzeug import secure_filename


ALLOWED_EXTENSIONS = set(['ped'])


# def allowed_file(filename):
#     return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS


def post_to_jobs():
    resp = Response(mimetype='application/json')
    if request.method != 'POST':
        resp.status_code = 405
        resp.set_data(json.dumps({"error": "Not a POST request."}))
    else:
        if "job_name" not in request.form or "ped_file" not in request.files:
            resp.status_code = 400
            resp.set_data(json.dumps({"error": "Invalid file."}))
        else:
            db = sql_pool.get_conn()
            db.begin()
            try:
                cur = db.cursor()
                job_id = str(uuid.uuid4())
                cur.execute("INSERT INTO jobs (id, name, user_id) VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s)", (job_id, request.form["job_name"], 1))
                sql = """
                    INSERT INTO status_changes (job_id, status_id)
                    SELECT UNHEX(REPLACE(%s,'-','')), statuses.id
                    FROM statuses
                    WHERE statuses.name = 'created'
                    LIMIT 1"""
                cur.execute(sql, (job_id,))
                db.commit()
            except:
                db.rollback()
                raise

            ped_file = request.files["ped_file"];
            job_directory = os.path.join(current_app.config.get("UPLOAD_FOLDER", "./"), job_id)
            os.mkdir(job_directory)
            ped_file.save(os.path.join(job_directory, "input.ped"))
            #TODO: Run validation
            #TODO: Queue Job
    return resp

def get_jobs():
    resp = Response(mimetype='application/json')
    return resp

def get_job(job_id):
    resp = Response(mimetype='application/json')
    return resp



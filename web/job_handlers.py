import os
import shutil
from flask import render_template, request, json, Response, current_app, redirect, send_file, make_response
from flask_login import current_user
import sql_pool
import MySQLdb
import uuid
import subprocess
import tabix
import gzip
import glob
import re
import time
#from werkzeug import secure_filename

def get_job_list_view():
    return render_template("job_list.html")

def post_to_jobs():
    resp = Response(mimetype="application/json")
    db = sql_pool.get_conn()
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

                    with open(ped_file_path) as f:
                        ped_header_line = f.readline()

                    ped_column_names = ped_header_line.strip().split()

                    kin_file = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), "vcf_kinship.kin")



                    analysis_cmd = current_app.config.get("ANALYSIS_BINARY", "epacts") + " single"
                    analysis_cmd += " --vcf " + current_app.config.get("VCF_FILE")
                    analysis_cmd += " --ped " + ped_file_path
                    analysis_cmd += " --min-maf 0.001 --field GT"
                    analysis_cmd += " --sepchr"
                    analysis_cmd += " --unit 500000 --test q.emmax"
                    analysis_cmd += " --kin " + kin_file
                    analysis_cmd += " --out ./output"
                    analysis_cmd += " --run 48"

                    batch_script_path = os.path.join(job_directory, "batch_script.sh")

                    with open(batch_script_path, "w+") as f:
                        f.write("#!/bin/bash\n")
                        f.write("#SBATCH --job-name=gasp_" + job_id + "\n")
                        f.write("#SBATCH --cpus-per-task=48\n")
                        f.write("#SBATCH --workdir=" + job_directory + "\n")
                        f.write("#SBATCH --mem-per-cpu=4000\n")
                        f.write("#SBATCH --time=14-0\n")
                        f.write("#SBATCH --nodes=1\n")
                        f.write("\n")
                        f.write(analysis_cmd + " 2> ./err.log 1> ./out.log\n")
                        f.write("EXIT_STATUS=$?\n")
                        f.write("if [ $EXIT_STATUS == 0 ]; then\n")
                        f.write("  " + current_app.config.get("MANHATTAN_BINARY") + " ./output.epacts.gz ./manhattan.json\n")
                        f.write("  " + current_app.config.get("QQPLOT_BINARY") + " ./output.epacts.gz ./qq.json\n")
                        f.write("fi\n")
                        f.write("echo $EXIT_STATUS > ./exit_status.txt\n")
                        f.write("exit $EXIT_STATUS\n")

                    if subprocess.call(current_app.config.get("QUEUE_JOB_BINARY", "sbatch") + " " + batch_script_path + " > " + os.path.join(job_directory, "batch_script_output.txt"), shell=True):
                        resp.status_code = 500
                        resp.set_data(json.dumps({"error": "An error occurred while scheduling job."}))
                    else:
                        cur = db.cursor()
                        cur.execute("""
                            INSERT INTO jobs (id, name, user_id, status_id)
                            VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s, (SELECT id FROM statuses WHERE name = 'queued'))
                            """, (job_id, request.form["job_name"], current_user.rid))
                        db.commit()
                        resp.set_data(json.dumps({"id": job_id}))
    return resp


def get_jobs():
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    cur = db.cursor(MySQLdb.cursors.DictCursor)
    sql = """
        SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(jobs.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date, DATE_FORMAT(jobs.modified_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS modified_date
        FROM jobs
        LEFT JOIN statuses ON jobs.status_id = statuses.id
        WHERE jobs.user_id = %s
        ORDER BY jobs.creation_date DESC
        """
    cur.execute(sql, (current_user.rid,))
    results = cur.fetchall()
    resp.set_data(json.dumps(results))
    return resp

def get_all_jobs():
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    cur = db.cursor(MySQLdb.cursors.DictCursor)
    sql = """
        SELECT bin_to_uuid(jobs.id) AS id, jobs.name AS name, statuses.name AS status, DATE_FORMAT(jobs.creation_date, '%Y-%m-%d %H:%i:%s') AS creation_date, DATE_FORMAT(jobs.modified_date, '%Y-%m-%d %H:%i:%s') AS modified_date,
        users.email as user_email
        FROM jobs
        LEFT JOIN statuses ON jobs.status_id = statuses.id
        LEFT JOIN users ON jobs.user_id = users.id
        ORDER BY jobs.creation_date DESC
        """
    cur.execute(sql)
    results = cur.fetchall()
    resp.set_data(json.dumps(results))
    return resp


def get_job_chunks(job_id):
    job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
    output_file_glob = os.path.join(job_directory, "output.*.epacts")
    files = glob.glob(output_file_glob)
    if len(files):
        chunks = []
        p = re.compile(r'output.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.epacts$')
        print files
        for file in files:
            m = p.search(file)
            chunk = dict(m.groupdict())
            chunk['start'] = int(chunk['start'])
            chunk['stop'] = int(chunk['stop'])
            chunk['modified'] = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(os.path.getmtime(file)))
            chunks.append(chunk)
        return chunks
    else:
        return []

def cancel_job(job_id):
    db = sql_pool.get_conn()
    resp = Response(mimetype='application/json')

    cur = db.cursor(MySQLdb.cursors.DictCursor)
    sql = "SELECT id FROM jobs WHERE id = uuid_to_bin(%s) AND user_id = %s"
    cur.execute(sql, (job_id, current_user.rid))
    db.commit()
    if cur.rowcount == 0:
        resp.status_code = 404
        resp.status = "JOB NOT FOUND"
    else:
        job_id_path = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id, "/batch_script_output.txt")

        with open(job_id_path, 'r') as f:
            slurm_job_id = f.readline()

        if subprocess.call("scancel " + slurm_job_id):
            resp.status_code = 500
            resp.status = "JOB CANCELLATION FAILED"

    return resp


def get_job_details_view(job_id):
    db = sql_pool.get_conn()
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
    cur.execute(sql, (current_user.rid, job_id))

    if cur.rowcount == 0:
        return "Job does not exist.", 404
    else:
        job_data = cur.fetchone()

        return render_template("job_details.html", job=job_data)


def get_job_locuszoom_plot(job_id, region):
    db = sql_pool.get_conn()
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
    cur.execute(sql, (current_user.rid, job_id))

    if cur.rowcount == 0:
        return "Job does not exist.", 404
    else:
        job_data = cur.fetchone()

    return render_template("job_locuszoom.html", job=job_data, region=region)


def get_job_output(job_id, filename, as_attach=False, mimetype=None):
    try:
        job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
        output_file_path = os.path.join(job_directory, filename)
        return send_file(output_file_path, as_attachment=as_attach, mimetype=mimetype)
    except:
        return "File Not Found", 404


def get_job_zoom(job_id):
    resp = Response(mimetype='application/json')
    db = sql_pool.get_conn()
    header = []
    epacts_filename = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id, "output.epacts.gz")
    with gzip.open(epacts_filename) as f:
        header = f.readline().rstrip('\n').split('\t')
        if header[1] == "BEG":
            header[1] = "BEGIN"
    assert len(header) > 0
    tb = tabix.open(epacts_filename)
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
        json_response_data["CHROM"].append(r[header.index("#CHROM")])
        json_response_data["BEGIN"].append(r[header.index("BEGIN")])
        json_response_data["END"].append(r[header.index("END")])
        json_response_data["MARKER_ID"].append(r[header.index("MARKER_ID")])
        #json_response_data["NS"].append(r[4])
        #json_response_data["AC"].append(r[5])
        #json_response_data["CALLRATE"].append(r[6])
        json_response_data["MAF"].append(r[header.index("MAF")])
        json_response_data["PVALUE"].append(r[header.index("PVALUE")])
    resp.set_data(json.dumps(json_response_data))
    return resp

def get_admin_main_page():
    return render_template("admin_main.html")

def send_as_json(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

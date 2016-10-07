import os
import shutil
from flask import render_template, request, json, Response, current_app, redirect, send_file, make_response, url_for
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
from auth import access_job_page
from genotype import Genotype
from job import Job 
from slurm_epacts_job import SlurmEpactsJob

def get_home_view():
    return render_template("home.html")

def get_job_list_view():
    return render_template("job_list.html")

@access_job_page
def get_job(job_id, job=None):
    return json_resp(job.as_object())

def get_jobs():
    jobs = Job.list_all_for_user(current_user.rid, current_app.config)
    return json_resp(jobs)

def get_all_jobs():
    jobs = Job.list_all(current_app.config)
    return json_resp(jobs)

def get_job_chunks(job_id):
    job_directory = os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)
    output_file_glob = os.path.join(job_directory, "output.*.epacts")
    files = glob.glob(output_file_glob)
    now = time.strftime('%Y-%m-%d %H:%M:%S')
    if len(files):
        chunks = []
        p = re.compile(r'output.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.epacts$')
        for file in files:
            m = p.search(file)
            chunk = dict(m.groupdict())
            chunk['start'] = int(chunk['start'])
            chunk['stop'] = int(chunk['stop'])
            chunk['modified'] = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(os.path.getmtime(file)))
            chunks.append(chunk)
        return {"data": chunks, "now": now}
    else:
        return {"data":[], "now": now} 

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

def purge_job(job_id):
    result = Job.purge(job_id, current_app.config)
    if result["found"]:
        return json_resp(result)
    else:
        return json_resp(result), 404

@access_job_page
def get_job_details_view(job_id, job=None):
    return render_template("job_details.html", job=job.as_object())

@access_job_page
def get_job_locuszoom_plot(job_id, region, job=None):
    return render_template("job_locuszoom.html", job=job.as_object(), region=region)

@access_job_page
def get_job_output(job_id, filename, as_attach=False, mimetype=None, tail=None, head=None, job=None):
    try:
        output_file = job.relative_path(filename)
        if tail or head:
            if tail and head:
                return "Cannot specify tail AND head", 500
            cmd = "head" if head else "tail"
            count = tail or head
            p = subprocess.Popen([cmd, "-n", count, output_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            tail_data, tail_error = p.communicate()
            resp = make_response(tail_data)
            if as_attach:
                resp.headers["Content-Disposition"] = "attachment; filename={}".format(filename)
            if mimetype:
                resp.headers["Content-Type"] = mimetype
            return resp
        else:
            return send_file(output_file, as_attachment=as_attach, mimetype=mimetype)
    except Exception as e:
        print e
        return "File Not Found", 404

@access_job_page
def get_job_zoom(job_id, job=None):
    header = []
    epacts_filename = job.relative_path("output.epacts.gz")
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
    json_response_data["NS"] = []
    #json_response_data["AC"] = []
    #json_response_data["CALLRATE"] = []
    json_response_data["MAF"] = []
    json_response_data["PVALUE"] = []
    json_response_data["BETA"] = []
    for r in results:
        if r[header.index("PVALUE")] != "NA":
            json_response_data["CHROM"].append(r[header.index("#CHROM")])
            json_response_data["BEGIN"].append(r[header.index("BEGIN")])
            json_response_data["END"].append(r[header.index("END")])
            json_response_data["MARKER_ID"].append(r[header.index("MARKER_ID")])
            json_response_data["NS"].append(r[4])
            #json_response_data["AC"].append(r[5])
            #json_response_data["CALLRATE"].append(r[6])
            json_response_data["MAF"].append(r[header.index("MAF")])
            json_response_data["PVALUE"].append(r[header.index("PVALUE")])
            json_response_data["BETA"].append(r[header.index("BETA")])
    return json_resp(json_response_data)

@access_job_page
def get_job_share_page(job_id, job=None):
    return render_template("job_share.html", job=job)

def post_to_jobs():
    user = current_user
    job_desc = dict()
    if request.method != 'POST':
        return json_resp({"error": "NOT A POST REQUEST"}), 405
    form_data = request.form
    genotype_id = form_data["genotype"]
    phenotype_id = form_data["phenotype"]
    job_desc["genotype"] = genotype_id
    job_desc["phenotype"] = phenotype_id
    job_desc["name"] = form_data["job_name"]
    job_desc["response"] =  form_data["response"] 
    if form_data.get("response_invnorm", False):
        job_desc["response_invnorm"] = True
    job_desc["covariates"] =  form_data.getlist("covariates")
    job_desc["type"] = form_data["model"]
    job_id = str(uuid.uuid4())

    if not job_id:
        return json_resp({"error": "COULD NOT GENERATE JOB ID"}), 500
    job_directory =  os.path.join(current_app.config.get("JOB_DATA_FOLDER", "./"), job_id)

    job = SlurmEpactsJob(job_id, job_directory, current_app.config) 

    try:
        os.mkdir(job_directory)
        job_desc_file = os.path.join(job_directory, "job.json")
        with open(job_desc_file, "w") as outfile:
            json.dump(job_desc, outfile)
    except Exception:
        return json_resp({"error": "COULD NOT SAVE JOB DESCRIPTION"}), 500
    # file has been saved to disc
    try:
        job.submit_job(job_desc)
    except Exception as e:
        print e
        shutil.rmtree(job_directory)
        return json_resp({"error": "COULD NOT ADD JOB TO QUEUE"}), 500 
    # job submitted to queue
    try:
        db = sql_pool.get_conn()
        cur = db.cursor()
        cur.execute("""
            INSERT INTO jobs (id, name, user_id, status_id)
            VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s, (SELECT id FROM statuses WHERE name = 'queued'))
            """, (job_id, job_desc["name"], current_user.rid))
        cur.execute("""
            INSERT INTO job_users(job_id, user_id, created_by, role_id)
            VALUES (uuid_to_bin(%s), %s, %s, (SELECT id FROM job_user_roles WHERE role_name = 'owner'))
            """, (job_id, current_user.rid, current_user.rid))
        db.commit()
    except:
        shutil.rmtree(job_directory)
        return json_resp({"error": "COULD NOT SAVE TO DATABASE"}), 500 
    # everything worked
    return json_resp({"id": job_id, "url_job": url_for("get_job", job_id=job_id)})

def get_genotypes():
    genos = Genotype.list_all()
    def get_stats(x):
        s = Genotype.get(x["id"],current_app.config).getStats() 
        s["name"] = x["name"]
        s["creation_date"] = x["creation_date"]
        s["id"] = x["id"]
        return s
    stats = [get_stats(x) for x in genos]
    return json_resp(stats)

def get_model_build_view():
    return render_template("model_build.html")

def get_models():
    models = SlurmEpactsJob.available_models()
    return json_resp(models)

def get_admin_main_page():
    return render_template("admin_main.html", githash=current_app.config.get("git-hash", None))

def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp


import os
import shutil
from flask import render_template, request, json, Response, current_app, redirect, send_file, make_response, url_for
from flask_login import current_user
import sql_pool
import MySQLdb
import uuid
import subprocess
import collections
import tabix
import gzip
import glob
import re
import time
import numpy as np
from auth import check_view_job, check_edit_job, can_user_edit_job
from genotype import Genotype
from phenotype import Phenotype
from job import Job 
from user import User
from slurm_epacts_job import SlurmEpactsJob

def get_home_view():
    return render_template("home.html")

@check_view_job
def get_job(job_id, job=None):
    return json_resp(job.as_object())

def get_jobs():
    jobs = Job.list_all_for_user(current_user.rid, current_app.config)
    return json_resp(jobs)

def get_all_jobs():
    jobs = Job.list_all(current_app.config)
    return json_resp(jobs)

def get_all_users():
    users = User.list_all(current_app.config)
    return json_resp(users)

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
            chunk['chr'] =  chunk['chr'].replace("chr", "")
            chunk['start'] = int(chunk['start'])
            chunk['stop'] = int(chunk['stop'])
            chunk['modified'] = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(os.path.getmtime(file)))
            chunks.append(chunk)
        return {"data": chunks, "now": now}
    else:
        return {"data":[], "now": now} 

@check_edit_job
def cancel_job(job_id, job=None):
    if job is None:
        return json_resp({"error": "JOB NOT FOUND"}), 404
    else:
        slurmjob = SlurmEpactsJob(job_id, job.root_path, current_app.config) 
        try:
            slurmjob.cancel_job()
        except Exception as exception:
            print exception
            return json_resp({"error": "COULD NOT CANCEL JOB"}), 500 
    return json_resp({"message": "Job canceled"})

def purge_job(job_id):
    result = Job.purge(job_id, current_app.config)
    if result["found"]:
        return json_resp(result)
    else:
        return json_resp(result), 404

@check_edit_job
def update_job(job_id, job=None):
    result = Job.update(job_id, request.values)
    if result.get("updated", False):
        return json_resp(result)
    else:
        return json_resp(result), 450

@check_view_job
def get_job_details_view(job_id, job=None):
    pheno = Phenotype.get(job.meta.get("phenotype", ""), current_app.config)
    geno = Genotype.get(job.meta.get("genotype", ""), current_app.config)
    job_obj = job.as_object()
    if pheno is not None:
        job_obj["details"]["phenotype"] = pheno.as_object()
    if geno is not None:
        job_obj["details"]["genotype"] = geno.as_object()
    if can_user_edit_job(current_user, job):
        job_obj["can_edit"] = True
    else:
        job_obj["can_edit"] = False
    return render_template("job_details.html", job=job_obj)

@check_view_job
def get_job_locuszoom_plot(job_id, region, job=None):
    geno = Genotype.get(job.get_genotype_id(), current_app.config)
    return render_template("job_locuszoom.html", job=job.as_object(), build=geno.build, region=region)

@check_view_job
def get_job_variant_page(job_id, job=None):
    chrom = request.args.get("chrom", None)
    pos = int(request.args.get("pos", None))
    variant_id = request.args.get("variant_id", None)
    return render_template("job_variant.html", job=job.as_object(), 
        variant_id=variant_id, chrom=chrom, pos=pos)

@check_view_job
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

@check_view_job
def get_job_zoom(job_id, job=None):
    header = []
    epacts_filename = job.relative_path("output.epacts.gz")
    with gzip.open(epacts_filename) as f:
        header = f.readline().rstrip('\n').split('\t')
        if header[1] == "BEG":
            header[1] = "BEGIN"
        if header[0] == "#CHROM":
            header[0] = "CHROM"
    assert len(header) > 0
    chrom = request.args.get("chrom", "")
    start_pos = int(request.args.get("start_pos", "0"))
    end_pos = int(request.args.get("end_pos", "0"))

    if chrom == "":
        return json_resp({"header": {"variant_columns": header}})

    headerpos = {x:i for i,x in enumerate(header)}
    tb = tabix.open(epacts_filename)
    results = tb.query(chrom, start_pos, end_pos)
    json_response_data = dict()

    json_response_data["CHROM"] = []
    json_response_data["BEGIN"] = []
    json_response_data["END"] = []
    json_response_data["MARKER_ID"] = []
    json_response_data["NS"] = []
    json_response_data["PVALUE"] = []
    if "MAF" in headerpos:
        json_response_data["MAF"] = []
    if "MAF" in headerpos:
        json_response_data["BETA"] = []
    for r in results:
        if r[headerpos["PVALUE"]] != "NA":
            json_response_data["CHROM"].append(r[headerpos["CHROM"]])
            json_response_data["BEGIN"].append(r[headerpos["BEGIN"]])
            json_response_data["END"].append(r[headerpos["END"]])
            json_response_data["MARKER_ID"].append(r[headerpos["MARKER_ID"]])
            json_response_data["PVALUE"].append(r[headerpos["PVALUE"]])
            json_response_data["NS"].append(r[4])
            if "MAF" in headerpos:
                json_response_data["MAF"].append(r[headerpos["MAF"]])
            if "BETA" in headerpos:
                json_response_data["BETA"].append(r[headerpos["BETA"]])
    return json_resp({"header": {"variant_columns": json_response_data.keys()}, \
        "data": json_response_data})

def merge_info_stats(info, info_stats):
    info_extract = re.compile(r'([A-Z0-9_]+)(?:=([^;]+))?(?:;|$)')
    matches = info_extract.findall(info)
    merged = dict()
    if "fields" in info_stats:
        merged[".fieldorder"] = info_stats["fields"]
    for match in matches:
        field = match[0]
        value = match[1]
        if "desc" in info_stats and field in info_stats["desc"]:
            stats = dict(info_stats["desc"][field])
        else:
            stats = dict()
        stats["value"] = value
        merged[field] = stats
    return merged

@check_view_job
def get_job_variant_pheno(job_id, job=None):
    chrom = request.args.get("chrom", None)
    pos = request.args.get("pos", None)
    variant_id = request.args.get("variant_id", None)
    if (chrom is None or pos is None):
        return json_resp({"error": "MISSING REQUIRED PARAMETER (chrom, pos)"}), 405
    pos = int(pos)
    geno = Genotype.get(job.meta["genotype"], current_app.config)
    reader = geno.get_geno_reader(current_app.config)
    try:
        variant = reader.get_variant(chrom, pos, variant_id)
    except Exception as e:
        return json_resp({"error": "Unable to retrieve genotypes ({})".format(e)})
    info_stats = geno.get_info_stats()
    info = variant["INFO"]
    calls = variant["GENOS"]
    del variant["GENOS"]
    variant["INFO"] = merge_info_stats(info, info_stats)
    phenos = job.get_adjusted_phenotypes()
    call_pheno = collections.defaultdict(list) 
    for sample, value in phenos.iteritems():
        sample_geno = calls[sample]
        call_pheno[sample_geno].append(value)
    summary = {}
    for genotype, observations in call_pheno.iteritems():
        obs_array = np.array(observations)
        q1 = np.percentile(obs_array, 25)
        q3 = np.percentile(obs_array, 75)
        iqr = (q3-q1)*1.5
        outliers = [x for x in obs_array if x<q1-iqr or x>q3+iqr]
        upper_whisker = obs_array[obs_array<=q3+iqr].max()
        lower_whisker = obs_array[obs_array>=q1-iqr].min()
        summary[genotype] = {
            "min": np.amin(obs_array),
            "w1": lower_whisker,
            "q1": q1,
            "mean": obs_array.mean(),
            "q2": np.percentile(obs_array, 50),
            "q3": q3,
            "w3": upper_whisker,
            "max": np.amax(obs_array),
            "n":  obs_array.size,
            "outliers": outliers
        }
    return json_resp({"header": variant,
        "data": summary})

@check_edit_job
def get_job_share_page(job_id, job=None):
    return render_template("job_share.html", job=job)

def post_to_jobs():
    user = current_user
    if not user.can_analyze():
        return "User Action Not Allowed", 403
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
    job_desc["genopheno"] =  form_data.getlist("genopheno")
    job_desc["type"] = form_data["model"]
    job_desc["user_id"] = current_user.rid
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
            INSERT INTO jobs (id, name, user_id, geno_id, pheno_id, status_id)
            VALUES (uuid_to_bin(%s), %s, %s, uuid_to_bin(%s), uuid_to_bin(%s),
            (SELECT id FROM statuses WHERE name = 'queued'))
            """, (job_id, job_desc["name"], job_desc["user_id"], job_desc["genotype"], job_desc["phenotype"]))
        cur.execute("""
            INSERT INTO job_users(job_id, user_id, created_by, role_id)
            VALUES (uuid_to_bin(%s), %s, %s, (SELECT id FROM job_user_roles WHERE role_name = 'owner'))
            """, (job_id, job_desc["user_id"], job_desc["user_id"]))
        db.commit()
    except:
        shutil.rmtree(job_directory)
        return json_resp({"error": "COULD NOT SAVE TO DATABASE"}), 500 
    # everything worked
    return json_resp({"id": job_id, "url_job": url_for("get_job", job_id=job_id)})

@check_edit_job
def post_to_share_job(job_id, job=None):
    form_data = request.form
    add = form_data["add"].split(",") 
    drop = form_data["drop"].split(",") 
    for address in (x for x in add if len(x)>0):
        Job.share_add_email(job_id, address, current_user)
    for address in (x for x in drop if len(x)>0):
        Job.share_drop_email(job_id, address, current_user)
    return json_resp({"id": job_id, "url_job": url_for("get_job", job_id=job_id)})

def get_genotypes():
    genos = Genotype.list_all()
    def get_stats(x):
        s = Genotype.get(x["id"],current_app.config).get_stats() 
        s["name"] = x["name"]
        s["creation_date"] = x["creation_date"]
        s["build"] = x["build"]
        s["id"] = x["id"]
        return s
    stats = [get_stats(x) for x in genos]
    return json_resp(stats)

def get_genotype(geno_id):
    g = Genotype.get(geno_id, current_app.config)
    return json_resp(g.as_object())

def get_genotype_info_stats(geno_id):
    g = Genotype.get(geno_id, current_app.config)
    return json_resp(g.get_info_stats())

def get_model_build_view():
    if current_user.can_analyze():
        return render_template("model_build.html")
    else:
        return render_template("not_authorized_to_analyze.html")

def get_models():
    models = SlurmEpactsJob.available_models()
    return json_resp(models)

def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp


import os
import shutil
from flask import render_template, request, json, Response, current_app, redirect,  make_response, url_for
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

import sys, traceback

def get_home_view():
    return render_template("home.html")

@check_view_job
def get_job(job_id, job=None):
    return json_resp(job.as_object())

def get_all_jobs():
    jobs = Job.list_all(current_app.config)
    return json_resp(jobs)

def get_all_users():
    users = User.list_all(current_app.config)
    return json_resp(users)

def purge_job(job_id):
    result = Job.purge(job_id, current_app.config)
    if result["found"]:
        return json_resp(result)
    else:
        return json_resp(result), 404

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
    if job.meta.get("genome_build"):
        build = job.meta["genome_build"]
    else:
        geno = Genotype.get(job.get_genotype_id(), current_app.config)
        build = geno.build
    return render_template("job_locuszoom.html", job=job.as_object(), build=build, region=region)

@check_view_job
def get_job_variant_page(job_id, job=None):
    chrom = request.args.get("chrom", None)
    pos = int(request.args.get("pos", None))
    variant_id = request.args.get("variant_id", None)
    return render_template("job_variant.html", job=job.as_object(), 
        variant_id=variant_id, chrom=chrom, pos=pos)

@check_view_job
def get_job_results(job_id, filters=dict(), job=None):
    epacts_filename = job.relative_path("output.epacts.gz")
    with gzip.open(epacts_filename) as f:
        header = f.readline().rstrip('\n').split('\t')
        if header[1] == "BEG":
            header[1] = "BEGIN"
        if header[0] == "#CHROM":
            header[0] = "CHROM"
    assert len(header) > 0
    headerpos = {x:i for i,x in enumerate(header)}

    if filters.get("region", ""):
        tb = tabix.open(epacts_filename)
        indata = tb.query(chrom, start_pos, end_pos)
    else:
        indata = (x.split("\t") for x in gzip.open(epacts_filename))

    pass_tests = []
    if filters.get("non-monomorphic", False):
        if "AC" not in headerpos:
            raise Exception("Column AC not found")
        ac_index = headerpos["AC"]
        def mono_pass(row):
            if float(row[ac_index])>0:
                return True
            else:
                return False
        pass_tests.append(mono_pass)

    if "max-pvalue" in filters:
        if "PVALUE" not in headerpos:
            raise Exception("Column PVALUE not found")
        pval_index = headerpos["PVALUE"]
        thresh = float(filters.get("max-pvalue", 1))
        def pval_pass(row):
            if row[pval_index] == "NA":
                return False
            if float(row[pval_index])<thresh:
                return True
            else:
                return False
        pass_tests.append(pval_pass)

    def pass_row(row):
        if len(pass_tests)==0:
            return True
        for f in pass_tests:
            if not f(row):
                return False
        return True

    def generate():
        yield "\t".join(header) + "\n"
        next(indata) #skip header
        for row in indata:
            if pass_row(row):
                yield "\t".join(row)

    return Response(generate(), mimetype="text/plain")

@check_edit_job
def get_job_share_page(job_id, job=None):
    return render_template("job_share.html", job=job)

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

def get_model_build_view():
    if current_user.can_analyze:
        return render_template("model_build.html")
    else:
        return render_template("not_authorized_to_analyze.html")

def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

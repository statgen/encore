from flask import Blueprint, Response, json, render_template, current_app, request, send_file
from flask_login import current_user, login_required
from genotype import Genotype
from phenotype import Phenotype
from job import Job 
from user import User
from auth import check_view_job, check_edit_job, can_user_edit_job, access_pheno_page, check_edit_pheno, can_user_edit_pheno

user_area = Blueprint("user", __name__,
    template_folder="templates")

@user_area.before_request
@login_required
def before_request():
    # Just here to trigger the login_required before any request
    pass

@user_area.route("/")
def index():
    return render_template("home.html")

@user_area.route("/jobs", methods=["GET"])
def get_jobs():
    return redirect(url_for("user.index"))

@user_area.route("/jobs/<job_id>", methods=["GET"])
@check_view_job
def get_job(job_id, job=None):
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

@user_area.route("/jobs/<job_id>/output", methods=["get"])
@check_view_job
def get_job_output(job_id, job=None):
    return get_job_output(job, "output.epacts.gz", True)

@user_area.route("/jobs/<job_id>/output/<file_name>", methods=["get"])
@check_view_job
def get_job_output_file(job_id, file_name, job=None):
    return get_job_output(job, file_name, True)

@user_area.route("/jobs/<job_id>/locuszoom/<region>", methods=["GET"])
@check_view_job
def get_job_locuszoom_plot(job_id, region, job=None):
    if job.meta.get("genome_build"):
        build = job.meta["genome_build"]
    else:
        geno = Genotype.get(job.get_genotype_id(), current_app.config)
        build = geno.build
    return render_template("job_locuszoom.html", job=job.as_object(), build=build, region=region)

@user_area.route("/jobs/<job_id>/variant", methods=["GET"])
@check_view_job
def get_job_variant_page(job_id, job=None):
    chrom = request.args.get("chrom", None)
    pos = int(request.args.get("pos", None))
    variant_id = request.args.get("variant_id", None)
    return render_template("job_variant.html", job=job.as_object(), 
        variant_id=variant_id, chrom=chrom, pos=pos)

@user_area.route("/jobs/<job_id>/share", methods=["GET"])
@check_edit_job
def get_job_share_page(job_id, job=None):
    return render_template("job_share.html", job=job)

@user_area.route("/phenos", methods=["GET"])
def get_phenos():
    return render_template("pheno_list.html")

@user_area.route("/phenos/<pheno_id>", methods=["GET"])
@access_pheno_page
def get_pheno(pheno_id, pheno=None):
    pheno_obj = pheno.as_object()
    if can_user_edit_pheno(current_user, pheno):
        pheno_obj["can_edit"] = True
    return render_template("pheno_details.html", pheno=pheno_obj)

@user_area.route("/pheno-upload", methods=["GET"])
def get_pheno_upload():
    if current_user.can_analyze:
        return render_template("pheno_upload.html")
    else:
        return render_template("not_authorized_to_analyze.html")

@user_area.route("/model-build", methods=["GET"])
def get_model_build():
    if current_user.can_analyze:
        return render_template("model_build.html")
    else:
        return render_template("not_authorized_to_analyze.html")

def get_job_output(job, filename, as_attach=False, mimetype=None, tail=None, head=None):
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


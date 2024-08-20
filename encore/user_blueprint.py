from flask import Blueprint, Response, json, render_template, current_app, request, send_file
from flask_login import current_user, login_required
from .api_blueprint import calculate_overlaps
from .genotype import Genotype
from .phenotype import Phenotype
from .notice import Notice
from .job import Job 
from .user import User
from .access_tracker import AccessTracker
import sqlite3
from .auth import check_view_job, check_edit_job, can_user_edit_job, check_view_pheno, check_edit_pheno, can_user_edit_pheno

user_area = Blueprint("user", __name__,
    template_folder="templates")

@user_area.before_request
@login_required
def before_request():
    # Just here to trigger the login_required before any request
    pass

@user_area.route("/")
def index():
    phenos = Phenotype.list_all_for_user(current_user.rid)
    notices = list(Notice.list_current())
    return render_template("home.html", phenos=phenos, notices=notices)

@user_area.route("/jobs", methods=["GET"])
def get_jobs():
    return redirect(url_for("user.index"))

@user_area.route("/jobs/<job_id>", methods=["GET"])
@check_view_job
def get_job(job_id, job=None):
    print("from get job", job.as_object())
    pheno = Phenotype.get(job.get_phenotype_id(), current_app.config)
    geno = Genotype.get(job.get_genotype_id(), current_app.config)
    job_obj = job.as_object()
    owner = job.get_owner()
    if pheno is not None:
        job_obj["details"]["phenotype"] = pheno.as_object()
    if geno is not None:
        job_obj["details"]["genotype"] = geno.as_object()
    if can_user_edit_job(current_user, job):
        job_obj["can_edit"] = True
    else:
        job_obj["can_edit"] = False

    details = job_obj.get('details')

    # Access response within details
    response = details.get('response') if details else None
    multibatch = details.get('multibatch') if details else "N"

    # Print results

    print("multibatch:", multibatch)

    if response:
        response_length = len(response)
        if (multibatch == "N"):
            print("Length of response:", response_length)
            return render_template("job_details.html", job=job_obj, owner=owner)
        else:
            print("Length of response batch:", response_length)
            return render_template("job_details_batch.html", job=job_obj, owner=owner)
    AccessTracker.LogJobAccess(job_id, current_user.rid)



@user_area.route("/jobs/<job_id>/response", methods=["GET"])
@check_view_job
def get_reponse_result(job_id, job=None):
    print("its in the response ******************** ")
    pheno = Phenotype.get(job.get_phenotype_id(), current_app.config)
    geno = Genotype.get(job.get_genotype_id(), current_app.config)
    job_obj = job.as_object()
    owner = job.get_owner()
    if pheno is not None:
        job_obj["details"]["phenotype"] = pheno.as_object()
    if geno is not None:
        job_obj["details"]["genotype"] = geno.as_object()
    if can_user_edit_job(current_user, job):
        job_obj["can_edit"] = True
    else:
        job_obj["can_edit"] = False
    #print(job_obj)
    details = job_obj.get('details')
    job_id = request.args.get('jobid')
    responsearg = request.args.get('response')
    print("responsearg",responsearg)
    # Access response within details


    # Print results


    resplist =[]
    resplist.append(responsearg)
    job_obj['details']['response'] = resplist

    response = details.get('response') if details else None
    print("aftetr the update treposn is",response)
    multibatch = details.get('multibatch') if details else "N"


    if response:
        response_length = len(response)
        if (response_length == 1):
            print("Length of response:", response_length)
            return render_template("job_details.html", job=job_obj, owner=owner)
        else:
            return render_template("job_details_batch.html", job=job_obj, owner=owner)

    AccessTracker.LogJobAccess(job_id, current_user.rid)






@user_area.route("/jobs/<job_id>/output", methods=["get"])
@check_view_job
def get_job_output(job_id, job=None):
    file_name = job.get_output_primary_file()
    short_name = job_id.partition("-")[0]
    send_as = short_name + "-" + file_name
    return get_job_output(job, file_name, True, send_as=send_as)

@user_area.route("/jobs/<job_id>/output/<file_name>", methods=["get"])
@check_view_job
def get_job_output_file(job_id, file_name, job=None,response=None):
    print("inside get_job_output_file", response)

    short_name = job_id.partition("-")[0]
    send_as = short_name + "-" + file_name
    return get_job_output(job, file_name, True, send_as=send_as)

@user_area.route("/jobs/<job_id>/locuszoom/<region>", methods=["GET"])
@check_view_job
def get_job_locuszoom_plot(job_id, region, job=None):
    geno = Genotype.get(job.get_genotype_id(), current_app.config)
    build = geno.build
    ld_info = geno.get_ld_info(current_app.config)
    variant = request.args.get("variant", "")
    return render_template("job_locuszoom.html", job=job.as_object(),
        variant=variant, ld_info = ld_info or {}, build=build, region=region)

@user_area.route("/jobs/<job_id>/variant", methods=["GET"])
@check_view_job
def get_job_variant_page(job_id, job=None):
    chrom = request.args.get("chrom", None)
    pos = int(request.args.get("pos", None))
    variant_id = request.args.get("variant_id", None)
    db_file=current_app.config.get("EQTL_DB_FILEPATH", "./")
    print("db_file",db_file)
    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()
    sql_query = "SELECT phenotype_id, variant_id, pip, af, cs_id, tissue" \
                " FROM susieeqtl WHERE variant_id = ?"

# Execute the query with the id parameter
    cursor.execute(sql_query, (variant_id,))
    # Execute the query with the variant IDs as parameters
    #cursor.execute(query, variant_ids)
    rows = cursor.fetchall()

    conn.close()
    json_data = {"header": {"cols": ["pheno_id", "variant_id", "pip", "af", "cs_id", "tissue"]}, "data": []}

    # Convert each row into a dictionary and append to the "data" list
    for row in rows:

        data_row = {
            "variant_id": row[1],
            "chr": row[1].split('_')[0],
            "pheno_id": row[0],
            "pip": row[2],
            "af": row[3],
            "cs_id": row[4],
            "tissue": row[5]
        }
        json_data["data"].append(data_row)






    return render_template("job_variant.html", job=job.as_object(), 
        variant_id=variant_id, chrom=chrom, pos=pos,json_data=json_data )

@user_area.route("/jobs/<job_id>/share", methods=["GET"])
@check_edit_job
def get_job_share_page(job_id, job=None):
    return render_template("job_share.html", job=job)

@user_area.route("/phenos", methods=["GET"])
def get_phenos():
    return render_template("pheno_list.html")

@user_area.route("/phenos/<pheno_id>", methods=["GET"])
@check_view_pheno
def get_pheno(pheno_id, pheno=None):
    pheno_obj = pheno.as_object()
    pheno_obj["overlap"] = calculate_overlaps(pheno)
    if can_user_edit_pheno(current_user, pheno):
        pheno_obj["can_edit"] = True
    if "errors" in pheno_obj and len(pheno_obj["errors"]):
        pheno_obj["has_errors"] = True
    return render_template("pheno_details.html", pheno=pheno_obj)

@user_area.route("/pheno-upload", methods=["GET"])
def get_pheno_upload():
    if current_user.can_analyze:
        return render_template("pheno_upload.html")
    else:
        return render_template("not_authorized_to_analyze.html")

@user_area.route("/genos", methods=["GET"])
def get_genos():
    return render_template("geno_list.html")

@user_area.route("/genos/<geno_id>", methods=["GET"])
def get_geno(geno_id):
    geno = Genotype.get(geno_id, config=current_app.config)
    if geno:
        geno_obj = geno.as_object()
    else:
        geno_obj = None 
    return render_template("geno_details.html", geno=geno_obj)

@user_area.route("/collaborate", methods=["GET"])
def get_collaborators():
    return render_template("collaborators.html")

@user_area.route("/collaborate/with/<user_id>", methods=["GET"])
def get_collaborations_with(user_id):
    collaborator = current_user.get_collaborator(user_id)
    return render_template("collaborate_with.html", collaborator_id=user_id, collaborator=collaborator)

@user_area.route("/me/api-token", methods=["GET"])
def get_api_token():
    return render_template("api_token.html")


@user_area.route("/help", methods=["GET"])
def get_help():
    return render_template("help.html", user=current_user)

@user_area.route("/model-build", methods=["GET"])
def get_model_build():
    if current_user.can_analyze:
        return render_template("model_build.html")
    else:
        return render_template("not_authorized_to_analyze.html")


@user_area.route("/model-build_batch", methods=["GET"])
def get_model_build_batch():
    if current_user.can_analyze:
        return render_template("model_build_batch.html")
    else:
        return render_template("not_authorized_to_analyze.html")


def get_job_output(job, filename, as_attach=False, mimetype=None, tail=None, head=None, send_as=None):
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
            return send_file(output_file, mimetype=mimetype,
                as_attachment=as_attach, attachment_filename=send_as)
    except Exception as e:
        print(e)
        return "File Not Found", 404


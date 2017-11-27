import os
from flask import request, Response, Flask, render_template, session, send_from_directory, redirect, send_file, url_for
from flask_login import LoginManager, login_required, current_user, logout_user
import job_handlers
import pheno_handlers
import admin_handlers
import sign_in_handler
import job_tracking
from functools import wraps
import atexit
import subprocess
import requests

APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))
APP_STATIC_PATH = os.path.join(APP_ROOT_PATH, 'static')
APP_TEMPLATES_PATH = os.path.join(APP_ROOT_PATH, 'templates')

app = Flask(__name__)
app.url_map.strict_slashes = False

app.config.from_pyfile(os.path.join(APP_ROOT_PATH, "../flask_config.py"))
app.config["PROPAGATE_EXCEPTIONS"] = True
app.config["SEND_FILE_MAX_AGE_DEFAULT"] = 60*5 # seconds

login_manager = LoginManager()
login_manager.init_app(app)

def admin_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_admin():
            return "You do not have access", 403 
        return f(*args, **kwargs)
    return decorated_function


@login_manager.user_loader
def user_loader(email):
    return sign_in_handler.load_user(email)

@app.route('/favicon.ico')
def favicon():
    return app.send_static_file('favicon.ico')

@app.route("/")
@login_required
def index():
    return job_handlers.get_home_view()

@app.route("/sign-in", methods=["GET"])
@login_manager.unauthorized_handler
def get_sign_in():
    return sign_in_handler.get_sign_in_view("sign-in") 

@app.route("/sign-out", methods=["GET"])
def sign_out():
    logout_user()
    return redirect(url_for("index"))

@app.route("/api/users", methods=["POST"])
@login_required
@admin_required
def add_user():
    return admin_handlers.add_user() 

@app.route("/api/geno", methods=["GET"])
@login_required
def get_genotypes():
    return job_handlers.get_genotypes()

@app.route("/api/geno/<geno_id>", methods=["GET"])
@login_required
def get_genotype(geno_id):
    return job_handlers.get_genotype(geno_id)

@app.route("/api/geno/<geno_id>/info", methods=["GET"])
@login_required
def get_genotype_info_stats(geno_id):
    return job_handlers.get_genotype_info_stats(geno_id)

@app.route("/jobs", methods=["GET"])
@login_required
def get_jobs():
    return redirect(url_for("index"))


@app.route("/jobs/<job_id>", methods=["GET"])
@login_required
def get_job(job_id):
    return job_handlers.get_job_details_view(job_id)


@app.route("/jobs/<job_id>/output", methods=["get"])
@login_required
def get_job_output(job_id):
    return job_handlers.get_job_output(job_id, "output.epacts.gz", True)

@app.route("/jobs/<job_id>/results", methods=["get"])
@login_required
def get_job_results(job_id):
    filters = request.args.to_dict()
    return job_handlers.get_job_results(job_id, filters)

@app.route("/jobs/<job_id>/locuszoom/<region>", methods=["GET"])
@login_required
def get_job_locuszoom_plot(job_id, region):
    return job_handlers.get_job_locuszoom_plot(job_id, region)

@app.route("/jobs/<job_id>/variant", methods=["GET"])
@login_required
def get_job_variant_page(job_id):
    return job_handlers.get_job_variant_page(job_id)

@app.route("/jobs/<job_id>/share", methods=["GET"])
@login_required
def get_job_share_page(job_id):
    return job_handlers.get_job_share_page(job_id)

@app.route("/api/jobs", methods=["POST"])
@login_required
def post_api_jobs():
    return job_handlers.post_to_jobs()

@app.route("/api/jobs", methods=["GET"])
@login_required
def get_api_jobs():
    return job_handlers.get_jobs()

@app.route("/api/jobs-all", methods=["GET"])
@login_required
@admin_required
def get_api_jobs_all():
    return job_handlers.get_all_jobs()

@app.route("/api/jobs/<job_id>", methods=["GET"])
@login_required
def get_api_job(job_id):
    return job_handlers.get_job(job_id)

@app.route("/api/jobs/<job_id>", methods=["DELETE"])
@login_required
def retire_api_job(job_id):
    return job_handlers.retire_job(job_id)

@app.route("/api/jobs/<job_id>/purge", methods=["DELETE"])
@login_required
@admin_required
def purge_api_job(job_id):
    return job_handlers.purge_job(job_id)

@app.route("/api/jobs/<job_id>", methods=["POST"])
@login_required
def update_api_job(job_id):
    return job_handlers.update_job(job_id)

@app.route("/api/jobs/<job_id>/share", methods=["POST"])
@login_required
def post_api_job_share_request(job_id):
    return job_handlers.post_to_share_job(job_id)

@app.route("/api/jobs/<job_id>/resubmit", methods=["POST"])
@login_required
def post_api_job_resubmit_request(job_id):
    return job_handlers.resubmit_job(job_id)

@app.route("/api/jobs/<job_id>/cancel_request", methods=["POST"])
@login_required
def post_api_job_cancel_request(job_id):
    return job_handlers.cancel_job(job_id)


@app.route("/api/jobs/<job_id>/plots/qq", methods=["GET"])
@login_required
def get_api_job_qq(job_id):
    return job_handlers.get_job_output(job_id, "qq.json")


@app.route("/api/jobs/<job_id>/plots/manhattan", methods=["GET"])
@login_required
def get_api_job_manhattan(job_id):
    return job_handlers.get_job_output(job_id, "manhattan.json")


@app.route("/api/jobs/<job_id>/plots/zoom", methods=["GET"])
@login_required
def get_api_job_zoom(job_id):
    return job_handlers.get_job_zoom(job_id)

@app.route("/api/jobs/<job_id>/plots/pheno", methods=["GET"])
@login_required
def get_api_job_variant_pheno(job_id):
    return job_handlers.get_job_variant_pheno(job_id)

@app.route("/api/jobs/<job_id>/tables/top", methods=["GET"])
@login_required
def get_api_job_tophits(job_id):
    return job_handlers.get_job_output(job_id, "tophits.json", False)

@app.route("/api/jobs/<job_id>/progress", methods=["GET"])
@login_required
def get_api_job_progress(job_id):
   return job_handlers.get_job_progress(job_id)

@app.route('/api/lz/<resource>', methods=["GET", "POST"], strict_slashes=False)
@login_required
def get_api_annotations(resource):
    if resource == "ld-results":
        return requests.get('http://portaldev.sph.umich.edu/api/v1/pair/LD/results/', params=request.args).content
    elif resource == "gene":
        return requests.get('http://portaldev.sph.umich.edu/api/v1/annotation/genes/', params=request.args).content
    elif resource == "recomb":
        return requests.get('http://portaldev.sph.umich.edu/api/v1/annotation/recomb/results/', params=request.args).content
    elif resource == "constraint":
        return requests.post('http://exac.broadinstitute.org/api/constraint', data=request.form).content
    else:
        return "Not Found", 404

@app.route("/api/queue", methods=["GET"])
@app.route("/api/queue/<job_id>", methods=["GET"])
@login_required
def get_queue_status(job_id=None):
    return job_handlers.get_queue_status(job_id) 

@app.route("/jobs/<job_id>/plots/tmp-qq", methods=["GET"])
@login_required
def get_job_tmp_qq(job_id):
    return job_handlers.get_job_output(job_id, "output.epacts.qq.pdf", False)


@app.route("/jobs/<job_id>/plots/tmp-manhattan", methods=["GET"])
@login_required
def get_job_tmp_manhattan(job_id):
    return job_handlers.get_job_output(job_id, "output.epacts.mh.pdf", False)

@app.route("/phenos", methods=["GET"])
@login_required
def get_pheno_list():
    return pheno_handlers.get_pheno_list_view()

@app.route("/phenos/<pheno_id>", methods=["GET"])
@login_required
def get_pheno(pheno_id):
    return pheno_handlers.get_pheno_details_view(pheno_id)

@app.route("/pheno-upload", methods=["GET"])
@login_required
def get_pheno_upload():
    return pheno_handlers.get_pheno_upload_view()

@app.route("/api/pheno", methods=["GET"])
@login_required
def get_api_pheno_list():
    return pheno_handlers.get_phenos()

@app.route("/api/pheno/<pheno_id>", methods=["GET"])
@login_required
def get_api_pheno_detail(pheno_id):
    return pheno_handlers.get_pheno(pheno_id)

@app.route("/api/pheno/<pheno_id>", methods=["POST"])
@login_required
def update_api_pheno(pheno_id):
    return pheno_handlers.update_pheno(pheno_id)

@app.route("/api/pheno/<pheno_id>", methods=["DELETE"])
@login_required
def retire_api_pheno(pheno_id):
    return pheno_handlers.retire_pheno(pheno_id)

@app.route("/api/pheno/<pheno_id>/purge", methods=["DELETE"])
@login_required
@admin_required
def purge_api_pheno(pheno_id):
    return pheno_handlers.purge_pheno(pheno_id)

@app.route("/api/pheno", methods=["POST"])
@login_required
def post_api_pheno():
    return pheno_handlers.post_to_pheno()

@app.route("/model-build", methods=["GET"])
@login_required
def get_model_build():
    return job_handlers.get_model_build_view()

@app.route("/api/model", methods=["GET"])
@login_required
def get_api_models():
    return job_handlers.get_models()

@app.route("/admin", methods=["GET"])
@login_required
@admin_required
def get_admin_page():
    return admin_handlers.get_admin_main_page()

@app.route("/admin/users", methods=["GET"])
@login_required
@admin_required
def get_admin_user_page():
    return admin_handlers.get_admin_user_page()

@app.route("/admin/phenos", methods=["GET"])
@login_required
@admin_required
def get_admin_pheno_page():
    return admin_handlers.get_admin_pheno_page()

@app.route("/admin/log/<job_id>/<log_name>", methods=["GET"])
@login_required
@admin_required
def get_job_log(job_id, log_name):
    tail = request.args.get("tail", 0)
    head = request.args.get("head", 0)
    if log_name in ["err.log","out.log"]:
        return job_handlers.get_job_output(job_id, log_name, \
            mimetype="text/plain", tail=tail, head=head)
    else:
        return "Not Found", 404

@app.route("/api/users-all", methods=["GET"])
@login_required
@admin_required
def get_api_users_all():
    return job_handlers.get_all_users()

@app.route("/api/phenos-all", methods=["GET"])
@login_required
@admin_required
def get_api_phenos_all():
    return pheno_handlers.get_all_phenos()

@app.context_processor
def template_helpers():
    def guess_tab(path):
        if path.startswith("/geno"):
            return "geno"
        elif path.startswith("/pheno"):
            return "pheno"
        elif path.startswith("/jobs") or path == "/":
            return "job"
        elif path == "/admin":
            return "job"
        elif path.startswith("/admin/user"):
            return "user"
        elif path.startswith("/admin/phenos"):
            return "pheno"
        else:
            return ""

    def get_navigation_links(path, user=None):
        links = {"left": [], "right":[]}
        if path.startswith("/admin"):
            links["left"].append(("job", "Jobs", url_for("get_admin_page")))
            links["left"].append(("user", "Users", url_for("get_admin_user_page")))
            links["left"].append(("pheno", "Phenos", url_for("get_admin_pheno_page")))
            links["right"].append(("return","Return to App", url_for("index")))
        else:
            links["left"].append(("job", "Jobs", url_for("index")))
            links["left"].append(("pheno", "Phenotypes", url_for("get_pheno_list")))
            if (user is not None) and hasattr(user, "is_admin") and user.is_admin():
                links["right"].append(("admin","Admin", url_for("get_admin_page")))
        links["right"].append(("logout","Logout", url_for("sign_out")))
        return links

    return dict(guess_tab = guess_tab, 
        get_navigation_links = get_navigation_links)


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500

job_tracker = job_tracking.Tracker(30.0, job_tracking.DatabaseCredentials("localhost", app.config.get("MYSQL_USER"), app.config.get("MYSQL_PASSWORD"), app.config.get("MYSQL_DB")))
job_tracker.start()

def on_exit():
    job_tracker.cancel()

atexit.register(on_exit)

# track current version
try:
    app.config["git-hash"] = subprocess.check_output([app.config.get("GIT_BINARY","git"), "rev-parse", "HEAD"])
except:
    pass

if __name__ == "__main__":
    app.run(debug=True, use_reloader=False, port=8080, host="0.0.0.0");

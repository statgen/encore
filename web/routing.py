import os
from flask import request, Response, Flask, render_template, session, send_from_directory, redirect, send_file, url_for
from flask_login import LoginManager, login_required, current_user, logout_user
import job_handlers
import pheno_handlers
import admin_handlers
import sign_in_handler
from user_blueprint import user_area
from admin_blueprint import admin_area
from api_blueprint import api
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

app.register_blueprint(user_area)
app.register_blueprint(admin_area, url_prefix="/admin")
app.register_blueprint(api, url_prefix="/api")

login_manager = LoginManager()
login_manager.init_app(app)


@login_manager.user_loader
def user_loader(email):
    return sign_in_handler.load_user(email)

@app.route('/favicon.ico')
def favicon():
    return app.send_static_file('favicon.ico')

@app.route("/")
@login_required
def index():
    return render_template("home.html")

@app.route("/sign-in", methods=["GET"])
@login_manager.unauthorized_handler
def get_sign_in():
    return sign_in_handler.get_sign_in_view("sign-in") 

@app.route("/sign-out", methods=["GET"])
def sign_out():
    logout_user()
    return redirect(url_for("index"))

@app.route("/jobs/<job_id>/results", methods=["get"])
@login_required
def get_job_results(job_id):
    filters = request.args.to_dict()
    return job_handlers.get_job_results(job_id, filters)

@app.route('/api/lz/<resource>', methods=["GET", "POST"], strict_slashes=False)
@login_required
def get_api_annotations(resource):
    if resource == "ld-results":
        ldresp =  requests.get('http://portaldev.sph.umich.edu/api/v1/pair/LD/results/', params=request.args)
        if ldresp.status_code != 500:
            return ldresp.content
        else:
            empty = "{\"data\": {\"position2\": []}}"
            return empty
    elif resource == "gene":
        return requests.get('http://portaldev.sph.umich.edu/api/v1/annotation/genes/', params=request.args).content
    elif resource == "recomb":
        return requests.get('http://portaldev.sph.umich.edu/api/v1/annotation/recomb/results/', params=request.args).content
    elif resource == "constraint":
        return requests.post('http://exac.broadinstitute.org/api/constraint', data=request.form).content
    else:
        return "Not Found", 404

@app.context_processor
def template_helpers():
    def guess_tab(path):
        if path.startswith("/geno"):
            return "geno"
        elif path.startswith("/pheno"):
            return "pheno"
        elif path.startswith("/jobs") or path == "/":
            return "job"
        elif path.startswith("/admin/user"):
            return "user"
        elif path.startswith("/admin/phenos"):
            return "pheno"
        elif path.startswith("/admin"):
            return "job"
        else:
            return ""

    def get_navigation_links(path, user=None):
        links = {"left": [], "right":[]}
        if path.startswith("/admin"):
            links["left"].append(("job", "Jobs", url_for("admin.get_admin_page")))
            links["left"].append(("user", "Users", url_for("admin.get_admin_user_page")))
            links["left"].append(("pheno", "Phenos", url_for("admin.get_admin_pheno_page")))
            links["right"].append(("return","Return to App", url_for("index")))
        else:
            links["left"].append(("job", "Jobs", url_for("index")))
            links["left"].append(("pheno", "Phenotypes", url_for("user.get_phenos")))
            if (user is not None) and hasattr(user, "is_admin") and user.is_admin():
                links["right"].append(("admin","Admin", url_for("admin.get_admin_page")))
        links["right"].append(("logout","Logout", url_for("sign_out")))
        return links

    return dict(guess_tab = guess_tab, 
        get_navigation_links = get_navigation_links)


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500

job_tracker = job_tracking.Tracker(5*60.0, job_tracking.DatabaseCredentials("localhost", app.config.get("MYSQL_USER"), app.config.get("MYSQL_PASSWORD"), app.config.get("MYSQL_DB")))
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

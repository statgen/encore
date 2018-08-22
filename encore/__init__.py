from flask import request, Response, Flask, render_template, redirect, url_for
from user_blueprint import user_area
from admin_blueprint import admin_area
from api_blueprint import api, ApiResult, ApiException
from auth_blueprint import auth
import job_tracking
import atexit
import subprocess

def create_app(config=None):

    app = ApiFlask(__name__)
    app.url_map.strict_slashes = False

    if isinstance(config, basestring):
        app.config.from_pyfile(config)
    elif isinstance(config, dict):
        app.config.update(config)
    elif config is None:
        pass
    else:
        raise Exception("Unknown config type")

    app.config["PROPAGATE_EXCEPTIONS"] = True
    app.config["SEND_FILE_MAX_AGE_DEFAULT"] = 60*5 # seconds

    app.register_blueprint(user_area)
    app.register_blueprint(admin_area, url_prefix="/admin")
    app.register_blueprint(api, url_prefix="/api")

    app.register_blueprint(auth)
    from auth_blueprint import login_manager
    login_manager.init_app(app)

    from sql_pool import register_db
    register_db(app)

    register_helpers(app)
    register_info(app)

    launch_tracker(app.config)
    return app


def register_helpers(app):
    @app.route('/favicon.ico')
    def favicon():
        return app.send_static_file('favicon.ico')

    @app.context_processor
    def template_helpers():
        def guess_tab(path):
            if path.startswith("/geno"):
                return "geno"
            elif path.startswith("/pheno"):
                return "pheno"
            elif path.startswith("/jobs") or path == "/":
                return "job"
            elif path.startswith("/geno"):
                return "geno"
            elif path.startswith("/help"):
                return "help"
            elif path.startswith("/admin/user"):
                return "user"
            elif path.startswith("/admin/phenos"):
                return "pheno"
            elif path.startswith("/admin/counts"):
                return "counts"
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
                links["left"].append(("counts", "Counts", url_for("admin.get_admin_counts_page")))
                links["right"].append(("return","Return to App", url_for("user.index")))
            else:
                links["left"].append(("job", "Jobs", url_for("user.index")))
                links["left"].append(("pheno", "Phenotypes", url_for("user.get_phenos")))
                links["left"].append(("geno", "Genotypes", url_for("user.get_genos")))
                if (user is not None) and hasattr(user, "is_admin") and user.is_admin():
                    links["right"].append(("admin","Admin", url_for("admin.get_admin_page")))
            links["right"].append(("help","Help", url_for("user.get_help")))
            links["right"].append(("logout","Logout", url_for("auth.sign_out")))
            return links

        return dict(guess_tab = guess_tab, 
            get_navigation_links = get_navigation_links)

def register_info(app):
    try:
        git_hash = subprocess.check_output([app.config.get("GIT_BINARY","git"), "rev-parse", "HEAD"])
        app.config["git-hash"] = git_hash 
    except:
        pass

def launch_tracker(config):
    job_tracker = job_tracking.Tracker(5*60.0, \
        job_tracking.DatabaseCredentials("localhost", config.get("MYSQL_USER"), 
        config.get("MYSQL_PASSWORD"), config.get("MYSQL_DB")))
    job_tracker.start()
    atexit.register(lambda:job_tracker.cancel())

class ApiFlask(Flask):
    def __init__(self, *args, **kwds):
       super(ApiFlask, self).__init__(*args, **kwds)
       self.register_error_handler(ApiException, lambda err: err.to_result())

    def make_response(self, rv):
        if isinstance(rv, ApiResult):
            return rv.to_response()
        return Flask.make_response(self, rv)


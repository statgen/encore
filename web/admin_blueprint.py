from flask import Blueprint, Response, json, render_template, current_app, request
from flask_login import current_user, login_required
from user import User
from functools import wraps

admin_area = Blueprint("admin", __name__,
    template_folder="templates")

def admin_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_admin():
            return "You do not have access", 403 
        return f(*args, **kwargs)
    return decorated_function

@admin_area.before_request
@login_required
@admin_required
def before_request():
    # Just here to trigger the admin_required before any request
    pass

@admin_area.route("/")
def get_admin_page():
    return render_template("admin_main.html", githash=current_app.config.get("git-hash", None))

@admin_area.route("/users/", methods=["GET"])
def get_admin_user_page():
    return render_template("admin_users.html")

@admin_area.route("/phenos/", methods=["GET"])
def get_admin_pheno_page():
    return render_template("admin_phenos.html")

@admin_area.route("/log/<job_id>/<log_name>", methods=["GET"])
def get_job_log(job_id, log_name):
    tail = request.args.get("tail", 0)
    head = request.args.get("head", 0)
    if log_name in ["err.log","out.log"]:
        return job_handlers.get_job_output(job_id, log_name, \
            mimetype="text/plain", tail=tail, head=head)
    else:
        return "Not Found", 404

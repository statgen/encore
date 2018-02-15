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

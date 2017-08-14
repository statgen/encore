from flask import render_template, current_app
from flask_login import current_user

def get_admin_main_page():
    return render_template("admin_main.html", githash=current_app.config.get("git-hash", None))

def get_admin_user_page():
    return render_template("admin_users.html")

def get_admin_pheno_page():
    return render_template("admin_phenos.html")



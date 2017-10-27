from flask import Response, json, render_template, current_app, request
from flask_login import current_user
from user import User

def add_user():
    result = User.create(request.values)
    if result.get("created", False):
        result["user"] = result["user"].as_object()
        return json_resp(result)
    else:
        return json_resp(result), 450

def get_admin_main_page():
    return render_template("admin_main.html", githash=current_app.config.get("git-hash", None))

def get_admin_user_page():
    return render_template("admin_users.html")

def get_admin_pheno_page():
    return render_template("admin_phenos.html")


def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

import os
from flask import Flask, render_template, session, send_from_directory, redirect, send_file, url_for
from flask_login import LoginManager, login_required
import job_handlers
import sign_in_handler
import re
import job_tracking
import atexit

APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))
APP_STATIC_PATH = os.path.join(APP_ROOT_PATH, 'static')
APP_TEMPLATES_PATH = os.path.join(APP_ROOT_PATH, 'templates')

app = Flask(__name__)

app.config.from_pyfile(os.path.join(APP_ROOT_PATH, "../flask_config.py"))
app.config["PROPAGATE_EXCEPTIONS"] = True

login_manager = LoginManager()
login_manager.init_app(app)

@login_manager.user_loader
def user_loader(email):
    return sign_in_handler.user_loader(email)

@app.route('/favicon.ico')
def favicon():
    return app.send_static_file('favicon.ico')

@app.route("/")
def index():
    return redirect("/jobs")


@app.route("/sign-in", methods=["GET"])
@login_manager.unauthorized_handler
def get_sign_in():
    return sign_in_handler.get_sign_in_view("sign-in") 


@app.route("/api/vcf/statistics", methods=["GET"])
def get_api_vcf_statistics():
    # API endpoint not protected.
    try:
        output_file_path = os.path.join(app.config.get("JOB_DATA_FOLDER", "./"), "vcf_stats.json")
        return send_file(output_file_path, as_attachment=False)
    except:
        return "File Not Found", 404


@app.route("/jobs", methods=["GET"])
@login_required
def get_jobs():
    return job_handlers.get_job_list_view()


@app.route("/jobs/<job_id>", methods=["GET"])
@login_required
def get_job(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_details_view(job_id)


@app.route("/jobs/<job_id>/output", methods=["GET"])
@login_required
def get_job_output(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "output.epacts.gz", True)


@app.route("/jobs/<job_id>/locuszoom/<region>", methods=["GET"])
@login_required
def get_job_locuszoom_plot(job_id, region):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_locuszoom_plot(job_id, region)


@app.route("/api/jobs", methods=["POST"])
@login_required
def post_api_jobs():
    return job_handlers.post_to_jobs()


@app.route("/api/jobs", methods=["GET"])
@login_required
def get_api_jobs():
    return job_handlers.get_jobs()


@app.route("/api/jobs/<job_id>", methods=["GET"])
@login_required
def get_api_job(job_id):
    return job_handlers.get_job(job_id)


@app.route("/api/jobs/<job_id>/cancel_request", methods=["POST"])
@login_required
def post_api_job_cancel_request(job_id):
    return job_handlers.cancel_job(job_id)


@app.route("/api/jobs/<job_id>/plots/qq", methods=["GET"])
@login_required
def get_api_job_qq(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "qq.json", False)


@app.route("/api/jobs/<job_id>/plots/manhattan", methods=["GET"])
@login_required
def get_api_job_manhattan(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "manhattan.json", False)


@app.route("/api/jobs/<job_id>/plots/zoom", methods=["GET"])
@login_required
def get_api_job_zoom(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_zoom(job_id)

@app.route("/api/jobs/<job_id>/tables/top", methods=["GET"])
@login_required
def get_api_job_tophits(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "tophits.json", False)

@app.route("/jobs/<job_id>/plots/tmp-qq", methods=["GET"])
@login_required
def get_job_tmp_qq(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "output.epacts.qq.pdf", False)


@app.route("/jobs/<job_id>/plots/tmp-manhattan", methods=["GET"])
@login_required
def get_job_tmp_manhattan(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_output(job_id, "output.epacts.mh.pdf", False)


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500

job_tracker = job_tracking.Tracker(30.0, job_tracking.DatabaseCredentials("localhost", app.config.get("MYSQL_USER"), app.config.get("MYSQL_PASSWORD"), app.config.get("MYSQL_DB")))
job_tracker.start()

def on_exit():
    job_tracker.cancel()

atexit.register(on_exit)

if __name__ == "__main__":
    app.run(debug=True, use_reloader=False, port=8080, host="0.0.0.0");

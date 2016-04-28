import os
from flask import Flask, render_template, session, send_from_directory
import job_handlers
import re

APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))
APP_STATIC_PATH = os.path.join(APP_ROOT_PATH, 'static')
APP_TEMPLATES_PATH = os.path.join(APP_ROOT_PATH, 'templates')

app = Flask(__name__)

app.config.from_pyfile(os.path.join(APP_ROOT_PATH, "../flask_config.py"))

@app.route("/")
def index():
    return "Hello ooooooooo World!"


@app.route("/jobs", methods=["GET"])
def get_jobs():
    session["user_id"] = 1
    return render_template("job_list.html")

@app.route("/jobs/<job_id>", methods=["GET"])
def get_job(job_id):
    if not re.match("^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$", job_id):
        return "Not Found", 404
    else:
        return job_handlers.get_job_details_view(job_id)


@app.route("/api/jobs", methods=["POST"])
def post_api_jobs():
    return job_handlers.post_to_jobs()

@app.route("/api/jobs", methods=["GET"])
def get_api_jobs():
    return job_handlers.get_jobs()


@app.route("/api/jobs/<job_id>", methods=["GET"])
def get_api_job(job_id):
    return job_handlers.get_job(job_id)


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500


if __name__ == "__main__":
    app.run(debug=True)

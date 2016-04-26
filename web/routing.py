import os
from flask import Flask, render_template, send_from_directory
import job_handlers

app = Flask(__name__)

APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))   # refers to application_top
APP_STATIC_PATH = os.path.join(APP_ROOT_PATH, 'static')
APP_TEMPLATES_PATH = os.path.join(APP_ROOT_PATH, 'templates')

app.config.from_pyfile(os.path.join(APP_ROOT_PATH, "../flask_config.py"))

@app.route("/")
def index():
    return "Hello ooooooooo World!"


@app.route("/dashboard", methods=["GET"])
def get_dashboard():
    return render_template("dashboard.html")


@app.route("/api/jobs", methods=["POST"])
def post_api_jobs():
    return job_handlers.post_to_jobs()


@app.route("/api/job/<job_id>", methods=["GET"])
def get_api_jobs(job_id):
    return job_handlers.get_jobs(job_id)


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500


if __name__ == "__main__":
    app.run(debug=True)

import os
from flask import Flask, render_template, send_from_directory
import ped_upload

app = Flask(__name__)

APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))   # refers to application_top
APP_STATIC_PATH = os.path.join(APP_ROOT_PATH, 'static')
APP_TEMPLATES_PATH = os.path.join(APP_ROOT_PATH, 'templates')


@app.route("/")
def index():
    return "Hello ooooooooo World!"


@app.route("/dashboard")
def dashboard_view():
    return render_template("dashboard.html")


@app.route("/api/ped-files", methods=["POST"])
def handle_ped_file_upload():
    return ped_upload.handle_upload()


# @app.errorhandler(500)
# def internal_error(exception):
#     return render_template('500.html'), 500


app.debug = True
if __name__ == "__main__":
    app.run()

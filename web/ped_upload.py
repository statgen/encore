import os
from flask import Flask, request, json, Response
from werkzeug import secure_filename

app = Flask(__name__)

ALLOWED_EXTENSIONS = set(['ped'])

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS

def handle_upload():
    resp = Response(mimetype='application/json')
    if request.method != 'POST':
        resp.status_code = 405
        resp.set_data(json.dumps({"error": "Not a POST request."}))
    else:
        ped_file = request.files['ped_file']
        if not ped_file or not allowed_file(ped_file.filename):
            resp.status_code = 400
            resp.set_data(json.dumps({"error": "Invalid file."}))
        else:
            filename = secure_filename(ped_file.filename)
            #ped_file.save(os.path.join(app.config["UPLOAD_FOLDER"], filename))
            ped_file.save(os.path.join("./", filename))
            # TODO: Run validation
            # TODO: Queue Job
    return resp



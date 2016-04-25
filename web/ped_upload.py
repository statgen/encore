import os
from flask import Flask, request, json, Response, current_app
#from werkzeug import secure_filename


ALLOWED_EXTENSIONS = set(['ped'])


# def allowed_file(filename):
#     return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS


def handle_upload():
    resp = Response(mimetype='application/json')
    if request.method != 'POST':
        resp.status_code = 405
        resp.set_data(json.dumps({"error": "Not a POST request."}))
    else:
        if "ped_file" not in request.files:
            resp.status_code = 400
            resp.set_data(json.dumps({"error": "Invalid file."}))
        else:
            ped_file = request.files["ped_file"];
            ped_file.save(os.path.join(current_app.config.get("UPLOAD_FOLDER", "./"), "input.ped"))
            #TODO: Run validation
            #TODO: Queue Job
    return resp



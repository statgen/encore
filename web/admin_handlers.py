from flask import Response, json, request
from user import User

def add_user():
    result = User.create(request.values)
    if result.get("created", False):
        result["user"] = result["user"].as_object()
        return json_resp(result)
    else:
        return json_resp(result), 450

def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

from flask import Blueprint, render_template, request, json, current_app, redirect, session, url_for, redirect
from flask_login import LoginManager, logout_user, login_required, current_user
import requests
from rauth import OAuth2Service
from .user import User
from .access_tracker import AccessTracker
import flask_login
from . import sql_pool
import jwt
import datetime

googleinfo = requests.get("https://accounts.google.com/.well-known/openid-configuration")
google_params = googleinfo.json()

auth = Blueprint("auth", __name__)

login_manager = LoginManager()

def encode_auth_token(user_id, duration = "4hour"):
    now_time = datetime.datetime.utcnow()
    if duration == "4hour":
        expire_time = now_time + datetime.timedelta(hours=4, seconds=0)
    elif duration == "7day":
        expire_time = now_time + datetime.timedelta(days=7, seconds=0)
    else:
        raise Exception("Invalid token duration: %s".format(duration))
    try:
        payload = {
            'exp': expire_time,
            'iat': now_time,
            'sub': user_id
        }
        return jwt.encode(
            payload,
            current_app.config.get('JWT_SECRET_KEY'),
            algorithm='HS256'
        )
    except Exception as e:
        print(e)
        return None

def decode_auth_token(auth_token):
    try:
        payload = jwt.decode(auth_token, current_app.config.get('JWT_SECRET_KEY'))
        return payload['sub']
    except jwt.ExpiredSignatureError:
        return 'Signature expired. Please log in again.'
    except jwt.InvalidTokenError:
        return 'Invalid token. Please log in again.'

@login_manager.request_loader
def user_loader_from_request(request):
    auth_header = request.headers.get("Authorization")
    if auth_header and auth_header.startswith("Bearer "):
        auth_token = auth_header.split(" ")[1]
    else:
        auth_token = ""
    if auth_token:
        try:
            email = decode_auth_token(auth_token)
            user = load_user(email)
            if user:
                AccessTracker.LogAPIAccess(user.rid)
            return user
        except Exception as e:
            print(e)
    return None

@login_manager.user_loader
def user_loader(email):
    return load_user(email)

@auth.route("/sign-in", methods=["GET"])
def get_sign_in():
    if request.args.get("orig", None):
        session["post_login_page"] = request.args.get("orig")
    return get_sign_in_view("sign-in") 

@auth.route("/get-auth-token", methods=["GET"])
@login_required
def get_auth_token():
    duration = request.args.get("duration", "4hour")
    return encode_auth_token(current_user.email, duration)

@login_manager.unauthorized_handler
def unauthorized():
    if request.path.startswith("/api"):
        return "UNAUTHORIZED", 401
    else:
        orig = request.full_path
        if orig == "/?":
            orig = None
        return redirect(url_for("auth.get_sign_in", orig=orig))

@auth.route("/sign-out", methods=["GET"])
def sign_out():
    logout_user()
    return redirect(url_for("auth.get_sign_in"))

def load_user(email):
    db = sql_pool.get_conn()
    user = User.from_email(email, db)
    if user:
        #try:
        user.log_login(db) 
        #except:
        #    pass
        return user
    else:
        return None

def get_sign_in_view(target):
    signin_url = request.url_root + target
    oauth_service = OAuth2Service(
        name="google",
        client_id=current_app.config.get("GOOGLE_LOGIN_CLIENT_ID", None),
        client_secret=current_app.config.get("GOOGLE_LOGIN_CLIENT_SECRET", None),
        authorize_url=google_params.get("authorization_endpoint"),
        base_url=google_params.get("userinfo_endpoint"),
        access_token_url=google_params.get("token_endpoint"))

    if "code" in request.args:
        oauth_session = oauth_service.get_auth_session(
            data={"code": request.args["code"],
                  "grant_type": "authorization_code",
                  "redirect_uri": signin_url},
            decoder=json.loads)
        user_data = oauth_session.get("").json()
        user = load_user(user_data["email"])
        if user:
            if user.is_active():
                flask_login.login_user(user)
                redirect_to = session.pop("post_login_page", None)
                try:
                    endpoint, arguments = current_app.url_map.bind('localhost').match(redirect_to)
                except Exception as e:
                    redirect_to = None
                if redirect_to:
                    return redirect(redirect_to)
                else:
                    return redirect(url_for("user.index"))
            else:
                error_message = "Account not active ({})".format(user_data["email"])
                return render_template("/sign_in.html", error_message=error_message)
        else:
            error_message = "Not an authorized user ({})".format(user_data["email"])
            return render_template("/sign_in.html", error_message=error_message)
    elif "authorize" in request.args:
        return redirect(oauth_service.get_authorize_url(
            scope="email",
            response_type="code",
            prompt="select_account",
            redirect_uri=signin_url))
    else:
        return render_template("/sign_in.html")


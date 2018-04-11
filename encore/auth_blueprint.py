from flask import Blueprint, render_template, request, json, current_app, redirect, session, url_for, redirect
from flask_login import LoginManager, logout_user
import urllib2
from rauth import OAuth2Service
from user import User
import flask_login
import sql_pool

googleinfo = urllib2.urlopen("https://accounts.google.com/.well-known/openid-configuration")
google_params = json.load(googleinfo)

auth = Blueprint("auth", __name__)

login_manager = LoginManager()

@login_manager.user_loader
def user_loader(email):
    return load_user(email)

@auth.route("/sign-in", methods=["GET"])
def get_sign_in():
    return get_sign_in_view("sign-in") 

@login_manager.unauthorized_handler
def unauthorized():
    if request.path.startswith("/api"):
        return "UNAUTHORIZED", 401
    else:
        return redirect(url_for("auth.get_sign_in", orig=request.path))

@auth.route("/sign-out", methods=["GET"])
def sign_out():
    logout_user()
    return redirect(url_for("user.index"))

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
            flask_login.login_user(user)
            return redirect(url_for("user.index"))
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


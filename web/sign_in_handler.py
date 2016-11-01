from flask import render_template, request, json, current_app, redirect, session, url_for
import urllib2
from rauth import OAuth2Service
from user import User
import flask_login
import sql_pool

googleinfo = urllib2.urlopen("https://accounts.google.com/.well-known/openid-configuration")
google_params = json.load(googleinfo)

def load_user(email):
    user = User.from_email(email, sql_pool.get_conn())
    if user:
        return user
    else:
        return None

def get_sign_in_view(target):
    signin_url = request.url_root + target
    oauth_service = OAuth2Service(
        name="google",
        client_id=current_app.config["GOOGLE_LOGIN_CLIENT_ID"],
        client_secret=current_app.config["GOOGLE_LOGIN_CLIENT_SECRET"],
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
            return redirect(url_for("index"))
        else:
            return render_template("/sign_in.html", error_message="Not an authorized user")
    elif "authorize" in request.args:
        return redirect(oauth_service.get_authorize_url(
            scope="email",
            response_type="code",
            prompt="select_account",
            redirect_uri=signin_url))
    else:
        return render_template("/sign_in.html")


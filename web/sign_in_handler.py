from flask import render_template, request, json, current_app, redirect, session
import urllib2
from rauth import OAuth2Service
from user import User
import flask_login
import sql_pool

googleinfo = urllib2.urlopen("https://accounts.google.com/.well-known/openid-configuration")
google_params = json.load(googleinfo)

def user_loader(email):
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
            decoder = json.loads,
            verify = False)
        user_data = oauth_session.get("", verify=False).json()
        flask_login.login_user(user_loader(user_data["email"]))
        return redirect("/jobs")
    elif "authorize" in request.args:
        return redirect(oauth_service.get_authorize_url(
            scope="email",
            response_type="code",
            prompt="select_account",
            redirect_uri=signin_url))
    else:
        return render_template("/sign_in.html")


from flask import render_template, request, json, current_app, redirect, session
import urllib2
from rauth import OAuth2Service
from user import User
import sql_pool

googleinfo = urllib2.urlopen("https://accounts.google.com/.well-known/openid-configuration")
google_params = json.load(googleinfo)


def get_sign_in_view():
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
                  "redirect_uri": "http://localhost:5000/sign-in"},
            decoder = json.loads,
            verify = False)
        user_data = oauth_session.get("", verify=False).json()
        user = User.from_email(user_data["email"], sql_pool.get_conn())
        if user:
            session["user_email"] = user.email
            return redirect("/jobs")
        else:
            return render_template("/sign_in.html", error_message="User not authorized.")
    elif "authorize" in request.args:
        return redirect(oauth_service.get_authorize_url(
            scope="email",
            response_type="code",
            prompt="select_account",
            redirect_uri="http://localhost:5000/sign-in"))
    else:
        return render_template("/sign_in.html")


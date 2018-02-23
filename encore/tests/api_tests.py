import pytest
import flask_login
from encore import create_app
from encore.user import User

@pytest.fixture(scope="module")
def app(request):
    app = create_app()
    ctx = app.app_context()
    ctx.push()
    request.addfinalizer(ctx.pop)
    return app

@pytest.fixture(scope="module")
def test_client(request, app):
    client = app.test_client()
    client.__enter__()
    request.addfinalizer(lambda: client.__exit__(None, None, None))
    return client

@pytest.fixture(scope="module")
def test_client_user(request, app):
    client = app.test_client()
    client.__enter__()
    with client.session_transaction() as sess:
        sess["user_id"] = "user@umich.edu" 
        sess["_fresh"] = True
    request.addfinalizer(lambda: client.__exit__(None, None, None))
    return client

def test_home_anon(test_client):
    rv = test_client.get("/")
    assert b'please sign in' in rv.data
    assert rv.status_code == 200

def test_home_user(test_client_user):
    rv = test_client_auth.get("/")
    assert b'Welcome' in rv.data

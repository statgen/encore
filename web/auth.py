import inspect
from functools import wraps
from job import Job
from flask import current_app
from flask_login import current_user

#most of these are helper function
#there are the dectorators you should use
# - access_job_page (needs job_id and job=None parameters)
#      will inject job parameter if None
#      checks if current user has access to job data

def can_user_view_job(user, job):
    if not user or not job:
        return False
    user_id = user.rid
    if user_id == job.user_id:
        return True
    if user_id in (x["user_id"] for x in job.users):
        return True
    if user.is_admin():
        return True
    return False

# decorators...
def splat_args(f, asf):
    formals, pargs, pkwargs, defaults = inspect.getargspec(asf)
    @wraps(f)
    def inner(*args, **kwargs):
        kwargs.update({k:v for (k,v) in zip(formals, args)})
        return f(**kwargs)
    return inner

def inject_job(f):
    @wraps(f)
    def inner(*args, **kwargs):
        if len(args):
            raise Exception("unnamed args")
        if "job_id" in kwargs and not kwargs.get("job",None):
            kwargs["job"] = Job.get(kwargs["job_id"], current_app.config)
        else:
            raise Exception("required 'job_id' parameter not found")
        return f(**kwargs)
    return inner

def can_view_job_page(f):
    @wraps(f)
    def inner(*args, **kwargs):
        if len(args):
            raise Exception("unnamed args")
        job = kwargs["job"]
        user = current_user
        if can_user_view_job(user, job):
            return f(**kwargs)
        else:
            return "Unauthorized", 403
    return inner

def access_job_page(f):
    return splat_args(inject_job(can_view_job_page(f)), f)

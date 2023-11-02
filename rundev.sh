#!/bin/bash
# use FLASK_RUN_PORT=8888 to set port
source venv/bin/activate
export FLASK_APP=`pwd`/runweb.py
export FLASK_DEBUG=1
FLASK_RUN_PORT=8888
flask run --host=0.0.0.0 

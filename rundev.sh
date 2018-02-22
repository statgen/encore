#!/bin/bash
source venv/bin/activate
export FLASK_APP=`pwd`/runweb.py
export FLASK_DEBUG=1
flask run --host=0.0.0.0 --port=8080

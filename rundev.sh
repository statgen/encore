#!/bin/bash
source venv/bin/activate
FLASK_APP=$(pwd)/runweb.py
FLASK_DEBUG=1
export FLASK_APP
export FLASK_DEBUG
flask run --host=0.0.0.0 

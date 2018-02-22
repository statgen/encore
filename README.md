# Encore 

Encore allows scientists to test hypotheses using large scale sequencing data
while hiding many of the complicated technical issues associated with working
on terabyte scale data.  In addition to performing the most common association
tests for single variants and groups of rare variants, Encore enables users to
annotate, share and interact with results.  Access to Encore is controlled by a
white list of users who can upload phenotypes and request analyses.  All
per-variant and per-gene summary statistics are returned.  To maintain
confidentiality of research participant data, individual level data cannot be
downloaded.  By optimizing the most common analyses and providing rich ways to
interact with results, Encore offers an exciting platform to investigate
genetic hypotheses.

# Installing python modules

To install all the required python modules use

     # If using a virtual environment, activate it first
	 # source venv/bin/activate
     pip install -r requirements.txt

# Run dev instance

To run a simple dev instance of Encore, you can run

    ./rundev.sh

# Apache Configuration

Currently Encore is deployed using WSGI with apache. You can 
install apache and the WSGI module with

    apt-get install apache2 python-setuptools libapache2-mod-wsgi

A sample wsgi file is included at encore.wsgi.example. You should
copy this file to encore.wsgi and fill in the correct path
for your server.

A sample apache conf file is included at encore.conf.example. You should
copy this file to /etc/apache2/sites-available/encore.conf and
fill in the correct URL and path for your server.

You can enable the configuration with

    sudo a2ensite encore
    sudo systemctl restart apache2


# Building Executable tools

     mkdir build
	 cd build
	 cmake ..
	 make

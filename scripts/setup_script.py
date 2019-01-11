#!/usr/bin/python

import shlex
import subprocess

def install_packages():
    packages = ['apache2',
                'autoconf',
                'autotools-dev',
                'build-essential',
                'curl',
                'git',
                'libapache2-mod-wsgi',
                'libmysqlclient-dev',
                'libffi-dev',
                'libssl-dev',
                'mysql-client',
                'mysql-server',
                'python-pip',
                'python-setuptools',
                'unzip']

    subprocess.call(['sudo', 'apt-get', 'update'])
    subprocess.call(['sudo', 'DEBIAN_FRONTEND=noninteractive', 'apt-get', 'install', '-y'] + packages)

def setup_encore():
    # Download and unpack Encore source
    subprocess.call(shlex.split('mkdir -p /srv/encore'))
    subprocess.call(shlex.split('curl -L https://github.com/statgen/encore/archive/master.zip --output /tmp/encore.zip'))
    subprocess.call(shlex.split('unzip /tmp/encore.zip -d /tmp/'))
    subprocess.call(shlex.split('cp -r /tmp/encore-master/. /srv/encore/'))
    subprocess.call(shlex.split('rm -rf /tmp/'))

def install_python_requirements():
    # Install Encore requirements
    subprocess.call(['pip', 'install', '--upgrade', 'pip'])
    #TODO make sure in right dir for command below
    subprocess.call(['pip', 'install', '-r', '/srv/encore/requirements.txt'])

def setup_mysql():
    pass
    #TODO setup root password and apply schema

def setup_apache():
    pass

def main():
    install_packages()
    setup_encore()
    install_python_requirements()
    setup_mysql()


if __name__ == '__main__':
    main()

#!/usr/bin/python

import shlex
import subprocess

def install_packages():
    packages = [
                'httpd',
                'curl',
                'git',
                'mod_wsgi',
                'python36u-pip',
                'python36u-devel',
                'python36u-setuptools',
                'unzip']

    subprocess.call(['sudo', 'yum', 'update', '-y'])
    subprocess.call(['sudo', 'yum', 'install', 'epel-release', '-y'])

    install_python3()

    subprocess.call(['sudo', 'yum', 'install', '-y'] + packages)


def install_python3():
    subprocess.call(['sudo', 'yum', 'update', '-y'])
    subprocess.call(['sudo', 'yum', 'install', 'yum-utils', '-y'])
    subprocess.call(['sudo', 'yum', 'groupinstall', '-y', 'development'])
    subprocess.call(shlex.split('sudo yum -y install https://centos7.iuscommunity.org/ius-release.rpm'))
    subprocess.call(shlex.split('sudo yum -y install python36u'))


def setup_encore():
    subprocess.call(shlex.split('mkdir -p /srv/encore'))
    subprocess.call(shlex.split('curl -L https://github.com/statgen/encore/archive/master.zip --output /tmp/encore.zip'))
    subprocess.call(shlex.split('unzip /tmp/encore.zip -d /tmp/'))
    subprocess.call(shlex.split('cp -r /tmp/encore-master/. /srv/encore/'))
    subprocess.call(shlex.split('rm -rf /tmp/encore.zip /tmp/encore-master'))


def install_python_requirements():
    subprocess.call(['pip3.6', 'install', '--upgrade', 'pip'])
    subprocess.call(['pip3.6', 'install', '--upgrade', 'setuptools'])
    subprocess.call(['pip3.6', 'install', '-r', '/srv/encore/requirements.txt'])


def setup_mysql():
    subprocess.call(['sudo', 'mkdir', '-p', '/var/lib/mysqld'])
    subprocess.call(['sudo', 'chown', '-R', 'mysql:mysql', '/var/lib/mysqld'])
    subprocess.call(['sudo', 'usermod', '-d', '/var/lib/mysql/', 'mysql'])
    subprocess.call(shlex.split('sudo service mysql start'))
    subprocess.call(['mysql', '-u', 'root', '-e',
        "CREATE USER 'flask-user'@'localhost' IDENTIFIED BY 'flask-user-pass'"])
    subprocess.call(['mysql', '-u', 'root', '-e',
        "GRANT DELETE, INSERT, SELECT, UPDATE, EXECUTE ON encore.* TO 'flask-user'@'localhost'"])
    subprocess.call(['mysql', '-u', 'root', '-e',
        "DELETE FROM mysql.user WHERE User=''"])
    subprocess.call(['mysql', '-u', 'root', '-e',
        "FLUSH PRIVILEGES"])
    subprocess.call(['mysql', '-u', 'root', '-e',
        "ALTER USER 'root'@'localhost' IDENTIFIED BY 'test-pass'"])


def setup_apache():
    subprocess.call(['sudo', 'cp', '/srv/encore/encore.conf.example', '/etc/apache2/sites-enabled/encore.conf'])
    subprocess.call(['sudo', 'a2enmod', 'wsgi'])
    subprocess.call(['sudo', 'a2ensite', 'encore'])
    subprocess.call(['sudo', 'service', 'apache2', 'restart'])

def main():
    install_packages()
    setup_encore()
    install_python_requirements()
    #setup_mysql()
    #setup_apache()


if __name__ == '__main__':
    main()

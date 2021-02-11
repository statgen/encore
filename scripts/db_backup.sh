#!/bin/bash

REACH=31
DATE=/bin/date
DAY=$($DATE +%d)
MONTH=$($DATE +%m)
YEAR=$($DATE +%Y)
YESTERDAY=$($DATE --date="${YEAR}-${MONTH}-${DAY} -1day" +%Y-%m-%d)

BACKUP_LOCATION="/nfs/turbo/ph-encore-active/backups"
ENCORE_HOME="/srv/encore"

MYSQL_DB=$(grep MYSQL_DB ${ENCORE_HOME}/flask_config.py | sed 's/,*$//g' | cut -d: -f2 | xargs)
MYSQL_USER=$(grep MYSQL_USER ${ENCORE_HOME}/flask_config.py | sed 's/,*$//g' | cut -d: -f2 | xargs)
MYSQL_PASSWORD=$(grep MYSQL_PASSWORD ${ENCORE_HOME}/flask_config.py | sed 's/,*$//g' | cut -d: -f2 | xargs)

# Backup database
mysqldump --user=${MYSQL_USER} --password=${MYSQL_PASSWORD} \
  ${MYSQL_DB} > ${BACKUP_LOCATION}/mariadb-backup.$($DATE "+%Y.%m.%d").sql

# Delete old backups keeping 2 weeks worth of daily backups
LOOP=0
while [ $LOOP -lt $REACH ]
do
    let OFFSET=$LOOP+14
    PREV_DATE=$($DATE --date="${YESTERDAY} -${OFFSET}days" +%Y.%m.%d)
    FILE=${BACKUP_LOCATION}/mariadb-backup.${PREV_DATE}.sql

    if [ -f "$FILE" ]; then
        echo "Deleting old daily backup (keep rolling 14 days): ${FILE}"
        rm ${FILE}
        if [ "$?" != "0" ]
        then
            echo "ERROR: Failed to delete old daily backup: ${FILE}"
            RETVAL=1
        fi
    fi

    let LOOP=$LOOP+1
done

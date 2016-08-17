#/bin/sh

THIS_SCRIPT=$BASH_SOURCE

if [ "$BASH_SOURCE" = "$0" ]
        then
                echo "This script must be sourced!"
                exit 1
fi

source ./project-utils/maxenv.sh
source ./project-utils/config.sh

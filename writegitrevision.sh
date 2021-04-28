#!/bin/sh -eux


if /usr/bin/git diff --exit-code --quiet
then 
    ID="\""$(/usr/bin/git rev-parse HEAD)"\""
else 
    ID="\""$(/usr/bin/git rev-parse HEAD)"+\""
fi

if /usr/bin/grep $ID program/gitrevision.h -q -F
then 
    echo "Git revision unchanged"
else
    echo -n "#define GIT_REVISION_ID $ID" > program/gitrevision.h
fi

#echo -n "#define COMPILE_DATETIME \"" > program/gitrevision.h
#echo -n $(date "+%Y-%m-%d %H:%M:%S") >> program/gitrevision.h
#echo "\"" >> program/gitrevision.h


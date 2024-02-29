#!/bin/bash

#version rule:MAJORVERSION.MINORVERSION.REVISION-r(COMMIT_COUNT)-g(COMMIT_ID)

BASE=$(pwd)
echo $BASE

#major version
MAJORVERSION=1

#minor version
MINORVERSION=0

#reversion,now use commit count
REVISION=1

#modue name/
MODULE_NAME=MM-module-name:aml_audio_hal

#get all commit count
COMMIT_COUNT=$(git rev-list HEAD --count)
echo commit count $COMMIT_COUNT

#get current commit id
COMMIT_ID=$(git show -s --pretty=format:%h)
echo commit id $COMMIT_ID

#version rule string
VERSION_STRING=${MAJORVERSION}.${MINORVERSION}.${REVISION}-r${COMMIT_COUNT}-g${COMMIT_ID}

#create version header file
sed "s/@version@/\"${MODULE_NAME},version:${VERSION_STRING}\"/" audio_hal/audio_hal_version.h.in > $1/audio_hal_version.h
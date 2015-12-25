#!/bin/sh

SOURCE=${NX_RSYNC_SOURCE:-'enk.me'}

ARTIFACT_DIR=artifacts/qt/${qt.version}/${platform}/${arch}/${box}/${build.configuration}

mkdir -p $ARTIFACT_DIR
rsync -rtvL --delete rsync://$SOURCE/buildenv/$ARTIFACT_DIR $ARTIFACT_DIR/..

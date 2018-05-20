#!/bin/bash

mkdir test-dir-locfs test-mount-locfs
dd bs=4096 count=6000 if=/dev/zero of=test-dir-locfs/image
./mkfs-locfs test-dir-locfs/image

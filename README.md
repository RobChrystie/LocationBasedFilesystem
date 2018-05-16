# LocationBasedFilesystem
This is a novel file system for Linux which will be used to store a file with GPS metadata.

Testing:
mkdir test-dir-locfs test-mount-locfs
dd bs=4096 count=6000 if=/dev/zero of=test-dir-locfs/image
./mkfs-locfs test-dir-locfs/image
mount -o loop,owner,group,users -t locfs test-dir-locfs/image test-mount-locfs

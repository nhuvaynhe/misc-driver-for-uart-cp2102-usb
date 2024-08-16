#!/usr/bin/zsh

NFS_ROOT_PATH=${HOME}/linux-kernel-labs/modules/nfsroot/root/

make
cp *.ko $NFS_ROOT_PATH

echo "Copy success!"



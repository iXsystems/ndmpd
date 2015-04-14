#!/usr/bin/env python
#
# Marcelo Araujo <araujo@FreeBSD.org>
#
import os

# Set source host.
shost="172.17.32.92"
susername="marcelo"
spwd="marcelo"
spath="/tmp/"
# Set target host.
thost="172.17.32.92"
tusername="marcelo"
tpwd="marcelo"
tpath="/tmp/"

# Use ndmpdcopy.jar to test it.
ndmpdcopy="java -jar ndmpcopy.jar"
ndmpdcopy = ndmpdcopy+ " -srchost %s -srcuser %s -srcpass %s -srcpath %s -srcbutype dump -srcenv TYPE=dump -srcenv RECURSIVE=Y -srcenv LEVEL=0 -srcenv HIST=Y -srcenv SNAPSURE=n"%(shost,susername,spwd,spath)
ndmpdcopy = ndmpdcopy+ " -dsthost %s -dstuser %s -dstpass %s -dstpath %s -dstbutype dump -dstenv TYPE=dump -dstenv RECURSIVE=Y -dstenv LEVEL=0 -dstenv FILESYSTEM=%s "%(thost,tusername,tpwd,tpath,spath)

os.system(ndmpdcopy)

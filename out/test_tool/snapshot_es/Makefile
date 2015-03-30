
SRCPATH ?= /usr/src
LIBPATH ?= /root/workspace/lib_es_tmp

CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+=    -I${SRCPATH}/cddl/compat/opensolaris/include
CFLAGS+=    -I${SRCPATH}/cddl/compat/opensolaris/lib/libumem
CFLAGS+=    -I${SRCPATH}/sys/cddl/compat/opensolaris
CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/head
CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/lib/libumem/common
CFLAGS+=    -I${SRCPATH}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+=    -I${SRCPATH}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+=    -I${SRCPATH}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=    -I${SRCPATH}/sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+=    -DNEED_SOLARIS_BOOLEAN

LIBS+=  ${LIBPATH}/libzfs.so
LIBS+=  ${LIBPATH}/libzfs_core.so.2
LIBS+=  ${LIBPATH}/libgeom.so
LIBS+=  ${LIBPATH}/libbsdxml.so
LIBS+=  ${LIBPATH}/libsbuf.so
LIBS+=  ${LIBPATH}/libm.so
LIBS+=  ${LIBPATH}/libnvpair.so
LIBS+=  ${LIBPATH}/libuutil.so
LIBS+=  ${LIBPATH}/libutil.so
LIBS+=  ${LIBPATH}/libavl.so
LIBS+=  ${LIBPATH}/libpthread.so
LIBS+=  ${LIBPATH}/libumem.so
LIBS+=  ${LIBPATH}/libzpool.so
LIBS+=  ${LIBPATH}/libcrypto.so


all:
	clang -g -I. $(LIBS) $(CFLAGS) take.c -o snap

clean:
	rm take


PROG = ndmpd

SRCS=   src/ndmpd.c \
        src/ndmp_xdr.c \
	$(NDMPD_SRCS) \
	$(HANDLER_SRCS) \
	$(TLM_SRCS)

NDMPD_SRCS = src/ndmpd_func.c \
			src/ndmpd_base64.c \
			src/ndmpd_table.c \
			src/ndmpd_util.c \
			src/ndmpd_prop.c \
			src/ndmpd_tar_v3.c \
			src/ndmpd_callbacks.c \
			src/ndmpd_fhistory.c \
			src/ndmpd_dtime.c \
			src/ndmpd_snapshot.c  

HANDLER_SRCS = src/ndmpd_connect.c \
			  src/ndmpd_info.c \
			  src/ndmpd_data.c \
			  src/ndmpd_mover.c 
			  
TLM_SRCS = tlm/tlm_util.c \
		  tlm/tlm_buffers.c \
		  tlm/tlm_lib.c \
		  tlm/tlm_backup_reader.c \
		  tlm/tlm_restore_writer.c \
		  tlm/tlm_info.c \
		  tlm/tlm_hardlink.c


LDADD=	-lmd -lpthread -lc
MAN=
CFLAGS += -I. -I./include 
		   
PREFIX ?= /usr/local
DSTDIR  = ${PREFIX}/sbin 

.c.o:
	$(CC) ${STATIC_CFLAGS}  $(CFLAGS)  -c $< -o $@	
	
install: ${PROG}
	@mkdir -p ${DSTDIR}
	@cp $(PROG) ${DSTDIR}/
	@cp ndmpd.conf /usr/local/etc/

plist:
	@echo "@comment files"
	@echo "@cwd ${DSTDIR}"
	@echo ${PROG}
	@echo "@cwd /etc"
	@echo ndmpd.conf

.include <bsd.prog.mk>


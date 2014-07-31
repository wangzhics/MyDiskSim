DISKSIM_PREFIX=../src

DISKSIM_OBJ = ../src/disksim_intr.o ../src/disksim_cache.o ../src/disksim_pfsim.o ../src/disksim_pfdisp.o\
	../src/disksim_synthio.o ../src/disksim_iotrace.o ../src/disksim_iosim.o ../src/disksim_logorg.o\
	../src/disksim_redun.o ../src/disksim_ioqueue.o ../src/disksim_iodriver.o ../src/disksim_bus.o\
	../src/disksim_controller.o ../src/disksim_ctlrdumb.o ../src/disksim_ctlrsmart.o\
	../src/disksim_disk.o ../src/disksim_diskctlr.o ../src/disksim_diskcache.o ../src/disksim_diskmap.o\
	../src/disksim_diskmech.o ../src/disksim_stat.o


LDFLAGS = -lm
DEBUG_OFLAGS = -g
CC = gcc -Wall

all : disksim_debug

clean : 
	rm -f *.o disksim_debug

disksim_debug : disksim_debug.o disksim_rms.o disksim_exec.o 
	$(CC) ${DEBUG_OFLAGS} -o disksim_debug  disksim_debug.o disksim_rms.o disksim_exec.o ${DISKSIM_OBJ} ${LDFLAGS} 
	
disksim_exec.o : 
	$(CC) ${DEBUG_OFLAGS} -I.${DISKSIM_PREFIX} -c disksim_exec.c -o disksim_exec.o
	
disksim_rms.o :
	$(CC) ${DEBUG_OFLAGS} -c disksim_rms.c -o disksim_rms.o

disksim_debug.o :
	$(CC) ${DEBUG_OFLAGS} -c disksim_debug.c -o disksim_debug.o
	

	
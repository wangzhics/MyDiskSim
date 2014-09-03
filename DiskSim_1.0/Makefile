SUBDIRS=src debug 

all : 
	$(MAKE) -C src
	$(MAKE) -C debug
	
clean:
	for d in $(SUBDIRS); do \
		echo $(MAKE) -C $$d $@;\
		$(MAKE) -C $$d $@; \
	done
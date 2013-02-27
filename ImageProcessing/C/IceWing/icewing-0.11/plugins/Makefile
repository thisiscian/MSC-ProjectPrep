SUBDIRS = demo min min_cxx remotectrl shmdata

all install depend clean pack:
	@for subdir in $(SUBDIRS) ; do \
		(cd $$subdir; \
		 echo make $@ in $$subdir "..."; \
		 $(MAKE) $@); \
	done

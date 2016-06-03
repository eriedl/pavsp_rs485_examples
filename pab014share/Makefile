
PRGS	= padec iFlow iPump iComII iPmon
IPRG	= aprs485
HLOG	= aprs485

%: %.c aprs485.h; gcc -O -Wall -I. -o $* $*.c -lm

all: $(PRGS) $(IPRG)

clean:; @l=`echo $(PRGS) $(IPRG)` ; for f in $$l ; do if [ -f $$f ] ; then rm -f $$f; fi ; done


# this is what I use....
INSDIR	= /usr/local/bin
VLOGDIR	= /var/log
HLOGDIR	= /home/log
UOWN	= root
UGRP	= bin
IFLAGS	= -o $(UOWN) -g $(UGRP) --backup=numbered
TGT	:= $(shell basename `pwd`)
PNM	:= $(shell echo $(TGT) | sed "s=[0-9]*==g")
BUPRT	= ~/BUP/$(PNM)

install: $(IPRG)
	@if [ `whoami` != "root" ] ; then echo YOU NEED TO BE root TO UPDATE INSTALLATION ; \
	else \
		for l in $(VLOG); do \
			dd=`echo $(VLOGDIR)/$$l`; \
			if [ -d $$dd ]; then echo "- ok -" $$dd; else \
			echo CREATE $$dd; install -o $(UOWN) -g $(UGRP) -m 0777 -d $$dd ; fi ; \
		done; \
		for l in $(HLOG); do \
			dd=`echo $(HLOGDIR)/$$l`; \
			if [ -d $$dd ]; then echo "- ok -" $$dd; else \
			echo CREATE $$dd; install -o $(UOWN) -g $(UGRP) -m 0777 -d $$dd ; fi ; \
		done; \
		dd=`echo $(INSDIR)` ; for f in $(IPRG) ; do \
			if cmp -s $$f $$dd/$$f; then echo "- ok -" $$dd/$$f; else \
			echo UPDATE $$dd/$$f; install $(IFLAGS) $$f $$dd ; fi ; \
		done ; \
	fi


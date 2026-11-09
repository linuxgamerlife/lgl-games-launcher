NAME := lgl-games-launcher
VERSION := $(shell sed -n 's/^Version:[[:space:]]*//p' packaging/$(NAME).spec | head -1)
TOP := $(CURDIR)/rpmbuild
SOURCES := $(TOP)/SOURCES
SPECS := $(TOP)/SPECS
RESULT := $(CURDIR)/result

.PHONY: all archive srpm rpm clean

all: rpm

$(SOURCES)/$(NAME)-$(VERSION).tar.gz:
	mkdir -p $(SOURCES) $(SPECS) $(RESULT)
	git archive --format=tar.gz --prefix=$(NAME)-$(VERSION)/ -o $@ HEAD
	cp packaging/$(NAME).spec $(SPECS)/

archive: $(SOURCES)/$(NAME)-$(VERSION).tar.gz

srpm: archive
	rpmbuild -bs $(SPECS)/$(NAME).spec --define "_topdir $(TOP)"
	mkdir -p $(RESULT)
	cp $(TOP)/SRPMS/$(NAME)-$(VERSION)-*.src.rpm $(RESULT)/

rpm: srpm
	rpmbuild -bb $(SPECS)/$(NAME).spec --define "_topdir $(TOP)"
	mkdir -p $(RESULT)
	cp $(TOP)/RPMS/*/$(NAME)-$(VERSION)-*.rpm $(RESULT)/
	cp $(SOURCES)/$(NAME)-$(VERSION).tar.gz $(RESULT)/

clean:
	rm -rf $(TOP) $(RESULT)

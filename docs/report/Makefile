export SHELL = /usr/bin/bash

TARGETS = $(patsubst %.tex, %, $(wildcard *.tex))

BUILD_DIR = build
OUTPUT_DIR = $(BUILD_DIR)/output
AUXILIA_DIR = $(BUILD_DIR)/auxilia
SCRATCH_DIR = $(BUILD_DIR)/_scratch_
TIKZ_SCRATCH_DIRS = $(SCRATCH_DIR)/tikz $(AUXILIA_DIR)/$(SCRATCH_DIR)/tikz
BUILD_DIRS = $(OUTPUT_DIR) $(AUXILIA_DIR) $(SCRATCH_DIR) $(TIKZ_SCRATCH_DIRS)

VIEW_EXE = xdg-open
LATEXMK = latexmk $(LATEXMK_FLAGS) -emulate-aux-dir -output-directory=$(OUTPUT_DIR) -aux-directory=$(AUXILIA_DIR)
#LATEXMK=latexmk -jobname=$(OUTPUT_PATH) # Do not use to redirect output to new directory
PDFLATEX_ARGS = '$$pdflatex=q/pdflatex %O -synctex=1 -shell-escape %S/'
PREVIEW_ARGS = '$$pdf_previewer=q{$(VIEW_EXE) %S 1>/dev/null 2>&1 &}'
CLEAN_OPTIONS = -e '$$bibtex_use=1.5' -e '$$clean_ext=q{%R.loa %R.auxlock %R.depends}'
CLEAN_OUTPUT_OPTIONS = $(CLEAN_OPTIONS) -e '$$clean_full_ext=q{%R.synctex.gz}'

.PHONY: all
all: $(TARGETS)

$(BUILD_DIR):
	mkdir --parents "$(BUILD_DIR)"

$(BUILD_DIR)/output: | $(BUILD_DIR)
	mkdir "$(BUILD_DIR)/output"

$(BUILD_DIR)/auxilia : | $(BUILD_DIR)
	mkdir "$(BUILD_DIR)/auxilia"

$(BUILD_DIR)/_scratch_: | $(BUILD_DIR)
	mkdir "$(BUILD_DIR)/_scratch_"

$(BUILD_DIR)/_scratch_/tikz: | $(BUILD_DIR)/_scratch_
	mkdir "$(BUILD_DIR)/_scratch_/tikz"

# Directory to hold the md5 hashes; must mirror location of tikzexternalize prefix option
$(BUILD_DIR)/auxilia/$(BUILD_DIR): | $(BUILD_DIR)/auxilia
	mkdir --parents "$(BUILD_DIR)/auxilia/$(BUILD_DIR)"

$(BUILD_DIR)/auxilia/$(BUILD_DIR)/_scratch_: | $(BUILD_DIR)/auxilia/$(BUILD_DIR)
	mkdir "$(BUILD_DIR)/auxilia/$(BUILD_DIR)/_scratch_"

$(BUILD_DIR)/auxilia/$(BUILD_DIR)/_scratch_/tikz: | $(BUILD_DIR)/auxilia/$(BUILD_DIR)/_scratch_
	mkdir "$(BUILD_DIR)/auxilia/$(BUILD_DIR)/_scratch_/tikz"

define latexmk
	$(eval _TARGET=$(1))
	$(eval _DEPENDENCY_LIST_FILE=$(2))
	$(eval _OPTIONS=$(subst $$, $$$$, $(3)))

	$(LATEXMK) $(_OPTIONS) -pdf -M -MP -MF $(_DEPENDENCY_LIST_FILE) $(_TARGET)
endef

.PHONY: $(TARGETS)
$(TARGETS): % : $(OUTPUT_DIR)/%.pdf

$(patsubst %, $(OUTPUT_DIR)/%.pdf, $(TARGETS)) : | $(BUILD_DIRS)
$(patsubst %, $(OUTPUT_DIR)/%.pdf, $(TARGETS)) : $(OUTPUT_DIR)/%.pdf : %.tex
	$(call latexmk, $<, $(patsubst %.tex, $(AUXILIA_DIR)/%.depends, $<), -e $(PDFLATEX_ARGS))

define declare_target
$(eval TARGET=$(1))

.PHONY: install-$(TARGET)
install-$(TARGET): $(TARGET).pdf

$(TARGET).pdf: $(OUTPUT_DIR)/$(TARGET).pdf
	cp $(OUTPUT_DIR)/$(TARGET).pdf $(TARGET).pdf

.PHONY: uninstall-$(TARGET)
uninstall-$(TARGET):
	if [ -f $(TARGET).pdf ]; \
	  then rm $(TARGET).pdf; \
	fi

.PHONY: view-$(TARGET)
view-$(TARGET): $(OUTPUT_DIR)/$(TARGET).pdf
	$(VIEW_EXE) $(OUTPUT_DIR)/$(TARGET).pdf 1>/dev/null 2>&1 &

.PHONY: trackview-$(TARGET)
trackview-$(TARGET): | $(BUILD_DIRS)
	$(call latexmk, $(TARGET).tex, $(AUXILIA_DIR)/$(TARGET).depends, -e $$(PDFLATEX_ARGS) -e $$(PREVIEW_ARGS) -pvc -view=pdf)

.PHONY: track-$(TARGET)
track-$(TARGET): | $(BUILD_DIRS)
	$(call latexmk, $(TARGET).tex, $(AUXILIA_DIR)/$(TARGET).depends, -e $$(PDFLATEX_ARGS) -pvc -view=none)

.PHONY: clean_target-$(TARGET)
clean_target-$(TARGET):
	if [ -f $(OUTPUT_DIR)/$(TARGET).pdf ]; \
	  then rm $(OUTPUT_DIR)/$(TARGET).pdf; \
	fi

.PHONY: clean_intermediary-$(TARGET)
clean_intermediary-$(TARGET):
	$(LATEXMK) $$(CLEAN_OPTIONS) -c $(TARGET).tex

.PHONY: clean_all-$(TARGET)
clean_all-$(TARGET):
	$(LATEXMK) $$(CLEAN_OUTPUT_OPTIONS) -C $(TARGET).tex
endef

$(foreach target, $(TARGETS), $(eval $(call declare_target, $(target))))

.PHONY: clean_intermediary clean_all clean_target install uninstall
clean_target: $(patsubst %, clean_target-%, $(TARGETS))
clean_intermediary: $(patsubst %, clean_intermediary-%, $(TARGETS))
clean_all: $(patsubst %, clean_all-%, $(TARGETS))
install: $(patsubst %, install-%, $(TARGETS))
uninstall: $(patsubst %, uninstall-%, $(TARGETS))

.PHONY: clean
clean:
	@echo "Removing files:"
	@if [ -d $(BUILD_DIR) ]; then \
	  find $(BUILD_DIR) -type f -print0 | xargs -0I % bash -c '{ echo "%"; rm "%"; }'; \
	  rm -r $(BUILD_DIR); \
	fi
	@echo 'Removing gnuplottex scratch directory:'
	@if [ -d gnuplottex ]; then \
	  ls -d gnuplottex; \
	  rm -r gnuplottex; \
	fi

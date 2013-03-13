PDF=pdflatex
BIB=bibtex
INPUT=main
OUTPUT=ProjectPrep-CianBooth-Report
OUTDIR=pdf
SUBFOLDER=tex
SUBFILES=$(patsubst $(SUBFOLDER)/%.tex,%,$(wildcard $(SUBFOLDER)/*.tex))

main:
	export TEXINPUTS=.:./tex/:..: && \
	$(PDF) -draftmode --output-directory $(OUTDIR) -jobname $(OUTPUT) $(INPUT) && \
	$(PDF) -draftmode --output-directory $(OUTDIR) -jobname $(OUTPUT) $(INPUT) && \
	$(BIB) $(OUTDIR)/$(OUTPUT) && \
	$(PDF) --output-directory $(OUTDIR) -jobname $(OUTPUT) $(INPUT)

quick_%: $(SUBFOLDER)/%.tex
	export TEXINPUTS=.:./tex/:..: && \
	cd $(SUBFOLDER) && \
  $(PDF) --output-directory ../$(OUTDIR) $(patsubst quick_%,%,$@)


%: $(SUBFOLDER)/%.tex
	export TEXINPUTS=.:./tex/:..: && \
	cd $(SUBFOLDER) && \
	$(PDF) -draftmode --output-directory ../$(OUTDIR) $@ && \
	cd .. && \
	$(BIB) $(OUTDIR)/$@ && \
  cd $(SUBFOLDER) && \
	$(PDF) -draftmode --output-directory ../$(OUTDIR) $@ && \
  $(PDF) --output-directory ../$(OUTDIR) $@

subfiles: $(SUBFILES)

clean:
	@rm $(OUTPUT).{pdf,aux,log,blg,bbl}

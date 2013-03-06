PDF=pdflatex
BIB=bibtex
INPUT=main
OUTPUT=ProjectPrep-CianBooth-Report
OUTDIR=pdf
SUBFOLDER=tex
SUBFILES=$(patsubst $(SUBFOLDER)/%.tex,%,$(wildcard $(SUBFOLDER)/*.tex))

main:
	export TEXINPUTS=.:./tex/:..: && \
	$(PDF) --output-directory $(OUTDIR) -jobname $(OUTPUT) $(INPUT)


%: $(SUBFOLDER)/%.tex
	export TEXINPUTS=.:./tex/:..: && \
  cd $(SUBFOLDER) && \
  $(PDF) --output-directory ../$(OUTDIR) $@

subfiles: $(SUBFILES)

bibtex:
	$(PDF) -jobname $(OUTPUT) $(INPUT)
	$(BIB) $(OUTPUT)
	$(PDF) -jobname $(OUTPUT) $(INPUT) 
	
clean:
	@rm $(OUTPUT).{pdf,aux,log,blg,bbl}

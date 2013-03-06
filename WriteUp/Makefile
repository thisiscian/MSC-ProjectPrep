PDF=pdflatex
BIB=bibtex
INPUT=main
OUTPUT=ProjectPrep-CianBooth-Report
SUBFOLDER=tex
SUBFILES=$(patsubst $(SUBFOLDER)/%.tex,%,$(wildcard $(SUBFOLDER)/*.tex))

main:
	$(PDF) -jobname $(OUTPUT) $(INPUT)


%: $(SUBFOLDER)/%.tex
	cd $(SUBFOLDER) && $(PDF) $@

subfiles: $(SUBFILES)

bibtex:
	$(PDF) -jobname $(OUTPUT) $(INPUT)
	$(BIB) $(OUTPUT)
	$(PDF) -jobname $(OUTPUT) $(INPUT) 
	
clean:
	@rm $(OUTPUT).{pdf,aux,log,blg,bbl}

PDF=pdflatex
BIB=bibtex
input=main
output=ProjectPrep-CianBooth-Report

main:
	$(PDF) -jobname $(output) $(input)

bibtex:
	$(PDF) -jobname $(output) $(input)
	$(BIB) $(output)
	$(PDF) -jobname $(output) $(input) 
	
clean:
	@rm $(output).{pdf,aux,log,blg,bbl}

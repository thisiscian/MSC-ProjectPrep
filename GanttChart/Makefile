PDF=pdflatex
BIB=bibtex
input=main
output=MSC-PP-Gantt

main:
	$(PDF) -jobname $(output) $(input)
	
clean:
	@rm $(output).{pdf,aux,log,blg,bbl}

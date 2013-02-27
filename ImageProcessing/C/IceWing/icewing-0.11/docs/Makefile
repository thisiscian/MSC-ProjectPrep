TEXS	= icewing.tex tex/install.tex tex/prgguide.tex tex/userguide.tex
IMGS	= images/iw_main.eps images/iw_render.eps images/iw_widgets.eps \
	images/menueContextMain.eps images/renderChain.eps \
	images/windowImages.eps images/windowMain.eps \
	images/winPlugData.eps images/winPlugFunctions.eps \
	images/winPlugLogging.eps images/winPlugObserver.eps \
	images/winPlugPlugins.eps images/iw_titel.eps

SRCS	= $(TEXS) $(IMGS)

icewing.pdf: $(SRCS)
	pdflatex -shell-escape icewing.tex

icewing.dvi: $(SRCS)
	latex icewing.tex

%.eps: %.png
	convert $< eps2:$@

%.eps: %.fig
	cd `dirname $<` ; \
	fig2dev -j -L eps `basename $<` `basename $@`

.PHONY: final

final: $(SRCS)
	pdflatex -shell-escape icewing.tex
	bibtex icewing
	makeindex -s index.ist icewing
	pdflatex -shell-escape icewing.tex
	pdflatex -shell-escape icewing.tex

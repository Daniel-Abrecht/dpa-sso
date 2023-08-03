
all: token.svg

%.svg: %.seqdiag
	seqdiag3 -T svg $^

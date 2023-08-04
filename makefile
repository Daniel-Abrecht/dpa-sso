
all: token.svg permission-token.svg

%.svg: %.seqdiag
	seqdiag3 -T svg $^

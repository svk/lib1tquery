TITAN_CFLAGS=-I/ltg/steinavk/lib/include
TITAN_LDFLAGS=-L/ltg/steinavk/lib/lib -Wl,-R/ltg/steinavk/lib/lib
UIO_CFLAGS=-I/uio/aristoteles/s38/steinavk/svk-ngram-tools/lib/include
UIO_LDFLAGS=-L/uio/aristoteles/s38/steinavk/svk-ngram-tools/lib/lib -Wl,-R/uio/aristoteles/s38/steinavk/svk-ngram-tools/lib/lib
CFLAGS=-std=c99 -g $(TITAN_CFLAGS) $(UIO_CFLAGS) $(OPTIMIZE_FLAGS) $(USE_ASSEMBLY_FLAGS) -fPIC
ASFLAGS=$(USE_ASSEMBLY_FLAGS)
CPPFLAGS=-g $(TITAN_CFLAGS) $(OPTIMIZE_FLAGS) $(USE_ASSEMBLY_FLAGS) -I ../lib/iniparser/src -fPIC
CC=gcc
LDFLAGS=-lm -lz -lJudy -liniparser $(TITAN_LDFLAGS) $(UIO_LDFLAGS) -L../lib/iniparser
CPPC=g++

GETOPTDIR=../lib/freegetopt-0.11
BINARIES=test-ngread test-judysort ../util/judysort-prepare-tapes ../util/mergetapes-to-sfbtin ../util/sfbtin-hashbins-lookup test-wordhash-j test-mergetapes

RAW_VOCABULARY=../data/vocab.gz
TRANSFORMER=../data/working/transformer.gz
PROCESSED_VOCABULARY=../data/working/vocabulary.gz
TEST_5GRAMS=../data/5gm-0015.gz
TEMP_TAPES=./temp-tapes/
TEST_TRANSFORMED_INPUT=../data/transformed.txt
TREE_DIR=../data/working/trees/
TEMP_DIR=/usit/sh/scratch/steinavk/tmp
NGRAM_DIR=../../ngrams/data/

JUDYSORT_ARGS=-L 100000000 -T $(TRANSFORMER) -W

all: $(BINARIES)

.PHONY: clean all util

clean:
	rm -f *.o
	rm -f a.out
	rm -f $(GETOPTDIR)/*.o
	rm -f $(BINARIES)

util: ../util/judysort-prepare-tapes ../util/mergetapes-to-sfbtin ../util/sfbtin-hashbins-lookup

$(TEMP_DIR)/tapes-2gram: ../util/judysort-prepare-tapes $(TRANSFORMER)
	mkdir -p $@
	zcat $(NGRAM_DIR)2gms/*.gz | ../util/judysort-prepare-tapes $(JUDYSORT_ARGS) -n 2 -P $(TEMP_DIR)/tapes-2gram/

$(TEMP_DIR)/tapes-3gram: ../util/judysort-prepare-tapes $(TRANSFORMER)
	mkdir -p $@
	zcat $(NGRAM_DIR)3gms/*.gz | ../util/judysort-prepare-tapes $(JUDYSORT_ARGS) -n 3 -P $(TEMP_DIR)/tapes-3gram/

$(TEMP_DIR)/tapes-4gram: ../util/judysort-prepare-tapes $(TRANSFORMER)
	mkdir -p $@
	zcat $(NGRAM_DIR)4gms/*.gz | ../util/judysort-prepare-tapes $(JUDYSORT_ARGS) -n 4 -P $(TEMP_DIR)/tapes-4gram/

$(TEMP_DIR)/tapes-5gram: ../util/judysort-prepare-tapes $(TRANSFORMER)
	mkdir -p $@
	zcat $(NGRAM_DIR)5gms/*.gz | ../util/judysort-prepare-tapes $(JUDYSORT_ARGS) -n 5 -P $(TEMP_DIR)/tapes-5gram/

tree-2gram: ../util/mergetapes-to-sfbtin
	../util/mergetapes-to-sfbtin -c -F -n 2 -B 20 -p 1 -o $(TREE_DIR)/2gram $(TEMP_DIR)/tapes-2gram/*.gz

tree-3gram: ../util/mergetapes-to-sfbtin
	../util/mergetapes-to-sfbtin -c -F -n 3 -B 10 -p 1 -o $(TREE_DIR)/3gram $(TEMP_DIR)/tapes-3gram/*.gz

tree-4gram: ../util/mergetapes-to-sfbtin
	../util/mergetapes-to-sfbtin -c -F -n 4 -B 7 -p 2 -o $(TREE_DIR)/4gram $(TEMP_DIR)/tapes-4gram/*.gz

tree-5gram: ../util/mergetapes-to-sfbtin
	../util/mergetapes-to-sfbtin -c -F -n 5 -B 5 -p 2 -o $(TREE_DIR)/5gram $(TEMP_DIR)/tapes-5gram/*.gz

$(TRANSFORMER): ../merging/transform.py ../merging/transform-vocab-unsorted.py $(RAW_VOCABULARY)
	python ../merging/transform-vocab-unsorted.py $(RAW_VOCABULARY) $(TRANSFORMER) $(PROCESSED_VOCABULARY)

$(PROCESSED_VOCABULARY): ../merging/transform.py ../merging/transform-vocab-unsorted.py $(RAW_VOCABULARY)
	python ../merging/transform-vocab-unsorted.py $(RAW_VOCABULARY) $(TRANSFORMER) $(PROCESSED_VOCABULARY)

test-sfbtin.output: $(TRANSFORMER) ../util/judysort-prepare-tapes ../util/mergetapes-to-sfbtin
	rm -rf $(TEMP_TAPES)
	rm -rf test_sfbtin.output
	mkdir $(TEMP_TAPES)
	zcat $(TEST_5GRAMS) | ./util/judysort-prepare-tapes -T $(TRANSFORMER) -W -n 5 -P $(TEMP_TAPES) -L 20000000
	../util/mergetapes-to-sfbtin -c -B 5 -o test-sfbtin.output -n 5 -p 2 $(TEMP_TAPES)/*

test-hash-binsize: test-hash-binsize.o sha1.o bins.o ngread.o $(GETOPTDIR)/getopt.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

test-ngread: test-ngread.o ngread.o $(GETOPTDIR)/getopt.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

test-wordhash-j: test-wordhash-c.o ngread.o jwordhash.o $(GETOPTDIR)/getopt.o
	$(CPPC) $(LDFLAGS) $(CFLAGS) -o $@ $^

../util/sfbtin-hashbins-lookup: sfbtin-hashbins-lookup.o jwordhash.o hashbin.o semifile.o sfbti.o ngread.o $(GETOPTDIR)/getopt.o tarbind.o
	$(CPPC) $(LDFLAGS) $(CPPFLAGS) -o $@ $^

test-judysort: test-judysort.o judysort.o
	$(CPPC) $(LDFLAGS) $(CPPFLAGS) -o $@ $^

test-iniparser: test-iniparser.o
	$(CPPC) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

../util/judysort-prepare-tapes: judysort-prepare-tapes.o judysort.o $(GETOPTDIR)/getopt.o jwordhash.o ngread.o
	$(CPPC) $(LDFLAGS) $(CPPFLAGS) -o $@ $^

test-mergetapes: test-mergetapes.o mergetapes.o mergetapes_asm.o
	$(CPPC) $(LDFLAGS) $(CPPFLAGS) -o $@ $^

../util/mergetapes-to-sfbtin: mergetapes-to-sfbtin.o mergetapes.o mergetapes_asm.o hashbin.o semifile.o sfbti.o ngread.o jwordhash.o $(GETOPTDIR)/getopt.o tarbind.o
	$(CPPC) $(LDFLAGS) $(CFLAGS) -o $@ $^

lib1tquery.so: lib1tquery.o hashbin.o jwordhash.o ngread.o sfbti.o tarbind.o
	$(CPPC) $(LDFLAGS) -shared $(CFLAGS) -o $@ -fPIC $^

test-lib1tquery: test-lib1tquery.o
	$(CPPC) -l1tquery $(LDFLAGS) $(CPPFLAGS) -o $@ $^

test-tarbind: test-tarbind.o tarbind.o
	$(CPPC) $(LDFLAGS) $(CFLAGS) -o $@ $^

cachekill: cachekill-readtest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

readtest: cachekill-readtest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

test-leafdump: test-leafdump.o
	$(CPPC) -l1tquery $(LDFLAGS) $(CPPFLAGS) -o $@ $^

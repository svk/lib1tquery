# A script to create lists of queries for performance-testing purposes.
# This script generates tests in a _fairly_ logical way (not something
#  arbitrary like using the first parts of each file):
#       - One pass is made through each n-gram. Each n-gram is included
#         with a certain probability
#       - Convert randomly to wildcards according to a certain probability
#         distribution (or never do this)

from __future__ import with_statement

def read_ngrams( args, counts = False ):
    import gzip
    for arg in args:
        f = gzip.open( arg, "r" )
        line = f.readline()
        while line:
            elements = line.split("\t")
            if counts:
                yield elements[0], int(elements[-1])
            else:
                yield elements[0]
            line = f.readline()
        f.close()

def count_entries( args ):
    rv = 0
    for ngram in read_ngrams( args ):
        rv += 1
    return rv

def make_independent_wildcarder( rate ):
    def independent_wildcarder( r, ngram ):
        rv = []
        for x in ngram:
            if r.random() < rate:
                rv.append( "<*>" )
            else:
                rv.append( x )
        return tuple( rv )
    return independent_wildcarder

class EntryProcessor:
    def __init__(self, rand, outFile, acceptRate, wildcarder, transformer, shuffle = False ):
        self.rand = rand
        self.outFile = outFile
        self.acceptRate = acceptRate
        self.wildcarder = wildcarder
        self.transformer = transformer
        self.rv = 0
        self.processed = 0
        self.collection = []
        self.shuffle = shuffle
    def feed( self, split_ngram ):
        if self.rand.random() < self.acceptRate:
            result = map( self.transformer, split_ngram )
            if self.wildcarder:
                result = self.wildcarder( self.rand, result )
            if self.shuffle:
                self.collection.append( result )
            else:
                self.output_ngram( result )
            self.rv += 1
        self.processed += 1
    def output_ngram( self, ngram ):
        print >> self.outFile, " ".join( ngram )
    def finalize( self ):
        if self.shuffle:
            self.rand.shuffle( ngram )
            for ngram in self.collection:
                self.output_ngram( ngram )
        self.outFile.close()
        return self.rv

#
#def process_entries( out, accept, args, wildcarder, progress, transformer, test_target = None ):
#    rv = 0
#    processed = 0
#    for ngram in read_ngrams( args ):
#        ngram = ngram.split(" ")
#        from sys import stderr
#        if accept():
#            out( wildcarder( map( transformer, ngram ) ) )
#            rv += 1
#            if test_target and rv >= test_target:
#                break
#        processed += 1
#        progress( processed )
#    return rv


if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option( "-o", "--output", dest="filename",
                       help="write queries to FILE", metavar="FILE",
                       default = None)
    parser.add_option( "-c", "--count", dest="count",
                       help="skip counting pass, there are NUM entries", metavar="NUM",
                       default = None )
    parser.add_option( "-1", "--count-1grams", action="store_true", dest="count_1grams", 
                       help="skip counting pass, use standard count for all the 1-grams (may be combined with the others)",
                       default = False )
    parser.add_option( "-2", "--count-2grams", action="store_true", dest="count_2grams", 
                       help="skip counting pass, use standard count for all the 2-grams (may be combined with the others)",
                       default = False )
    parser.add_option( "-3", "--count-3grams", action="store_true", dest="count_3grams", 
                       help="skip counting pass, use standard count for all the 3-grams (may be combined with the others)",
                       default = False )
    parser.add_option( "-4", "--count-4grams", action="store_true", dest="count_4grams", 
                       help="skip counting pass, use standard count for all the 4-grams (may be combined with the others)",
                       default = False )
    parser.add_option( "-5", "--count-5grams", action="store_true", dest="count_5grams", 
                       help="skip counting pass, use standard count for all the 5-grams (may be combined with the others)",
                       default = False )
    parser.add_option( "-q", "--queries", dest="queries_to_generate",
                       help="generate approximately NUM queries", metavar="NUM",
                       default = 1000000)
    parser.add_option( "-s", "--seed", dest="seed",
                       help="use NUM as random seed", metavar="NUM",
                       default = None )
    parser.add_option( "-r", "--random-shuffle", action="store_true", dest="shuffle",
                       help="shuffle the queries (keeping them in memory until the very end!)", metavar="NUM",
                       default = False )
    parser.add_option( "-w", "--wildcard-independent", dest = "wildcard_independent",
                       help="generate with NUM independent probability of every token becoming a wildcard", metavar="NUM",
                       default = None )
    parser.add_option( "-T", "--transformer", dest = "transformer",
                       help="preprocess using transformer", metavar="NUM",
                       default = None )
    parser.add_option( "-S", "--seeds", dest="seeds",
                       help="use NUMS as random seeds, generate filenames automatically", metavar = "NUM",
                       default = None )
    options, args = parser.parse_args()
    StandardCounts = {
        1: 13588391,
        2: 314843401,
        3: 977069902,
        4: 1313818354,
        5: 1176470663
    }
    from random import Random
    from sys import stdout, stderr
    from time import time
    r = Random()
    out = stdout
    if options.seeds and (options.seed or options.filename):
        parser.error( "--seeds excludes --seed and --output" )
    if options.seed:
        r = Random( options.seed )
    if options.filename:
        out = open( options.filename, "w" )
    if options.count:
        count = int(options.count)
    elif options.count_1grams or options.count_2grams or options.count_3grams or options.count_4grams or options.count_5grams:
        count = 0
        if options.count_1grams:
            count += StandardCounts[1]
        if options.count_2grams:
            count += StandardCounts[2]
        if options.count_3grams:
            count += StandardCounts[3]
        if options.count_4grams:
            count += StandardCounts[4]
        if options.count_5grams:
            count += StandardCounts[5]
        print >> stderr, "Counting %d n-grams." % count
    else:
        t0 = time()
        count = count_entries( args )
        t1 = time()
        print >> stderr, "Counted %d n-grams (took %0.2lf seconds)." % (count, t1-t0)
    wildcarder = lambda r, x : x
    if options.wildcard_independent:
        chance = float( options.wildcard_independent )
        wildcarder = make_independent_wildcarder( chance )
    transform = lambda x : x
    if options.transformer:
        d = {}
        print >> stderr, "Loading transformer from %s.." % options.transformer,
        import gzip
        try:
            f = gzip.open( options.transformer, "r" )
            for line in f:
                key, value = line.split( "\t" )
                d[ key ] = int( value )
        finally:
            f.close()
        print >> stderr, "done."
        transform = lambda x : d[x]
#    def print_ngram( ngram ):
#        s = " ".join( wildcarder( ngram ) ) # double wildcarding?
#        print >> out, s
#    output = out
#    collection = []
#    if options.shuffle:
#        def add_ngram_to_collection( x ):
#            collection.append( x )
#        output = None
    target = int(options.queries_to_generate)
    probability = target / float( count )
    print >> stderr, "Running with probability %lf." % probability
    t0 = time()
    def progress( n ):
        N = count
        n0 = (n*100)//N
        nn1 = ((n-1)*100)//N
        if n > 0 and n0 != nn1:
            elapsed = time() - t0
            speed = n / elapsed
            remaining = (N-n) / speed
            print "%d%% done, %0.lf seconds remaining.." % (n0, remaining)
    entryProcessors = []
    if not options.seeds:
        entryProcessor = EntryProcessor( r, outFile = out, acceptRate = probability, wildcarder = wildcarder, transformer = transform, shuffle = options.shuffle)
        entryProcessors.append( entryProcessor )
    else:
        l = [ (seed, "test-query-set-s%d-q%d.txt" % ( seed, target )) for seed in map( int, options.seeds.split(",") ) ]
        for seed, filename in l:
            r = Random( seed )
            f = open( filename, "w" )
            ep = EntryProcessor( r, outFile = f, acceptRate = probability, wildcarder = wildcarder, transformer = transform, shuffle = options.shuffle )
            entryProcessors.append( ep )
    print >> stderr, "Generating", len(entryProcessors), "in parallel.."
    processed = 0
    for ngram in read_ngrams( args ):
        split_ngram = ngram.split(" ")
        for ep in entryProcessors:
            ep.feed( split_ngram )
        processed += 1
        progress( processed )
    t1 = time()
    for ep in entryProcessors:
        generated = ep.finalize()
        print >> stderr, "Generated %d queries (%0.2lf of target, took %0.2lf seconds)." % (generated, (generated/float(target)*100.0), t1 - t0)

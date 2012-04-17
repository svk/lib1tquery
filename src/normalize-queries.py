from gzip import GzipFile
from sys import stderr

def read_twoway_transformer( f ):
    rv = {}
    vr = {}
    for line in f.readlines():
        if len(rv) % 10000 == 0:
            print >> stderr, "length now", len(rv)
        key, value = line.split()
        rv[key] = value
        vr[value] = key
#        if value not in vr.keys():
#            vr[value] = key
    return rv, vr

def read_test_queries( f, trans ):
    rv = []
    while True:
        line = f.readline()
        if not line:
            break
        t = map( trans, line.split() )
        for m in range(2**len(t)):
            c = []
            for i in range(len(t)):
                if ((2**i) & m) == 0:
                    c.append( t[i] )
                else:
                    c.append( "<*>" )
            rv.append( tuple(c) )
    return rv

def uniq_queries( qs ):
    return list( set( qs ) )

def randomize_queries( qs ):
    from random import shuffle
    shuffle( qs )

if __name__ == '__main__':
    from sys import argv, stderr
    tfile, qfile = argv[1], argv[2]
    print >> stderr, "Transformer file: " , tfile
    print >> stderr, "Query file: ", qfile
    tf = open( tfile, "r" )
    qf = open( qfile, "r" )
    print >> stderr, "Reading transformer"
    rv, vr = read_twoway_transformer( tf )
    tf.close()
    f = lambda x : vr[ rv[ x ] ]
    try:
        print >> stderr, rv[ "1948" ]
    except KeyError:
        print >> stderr, "not found"
    try:
        print >> stderr, vr[ rv[ "1948" ] ]
    except KeyError:
        print >> stderr, "not found"
    try:
        print >> stderr, rv[ "0000" ]
    except KeyError:
        print >> stderr, "not found"
    try:
        print >> stderr, vr[ rv[ "0000" ] ]
    except KeyError:
        print >> stderr, "not found"
    print >> stderr, "Reading and wildcarding queries"
    qs = read_test_queries( qf, f )
    qf.close()
    print >> stderr, "Making queries unique"
    qs = uniq_queries( qs )
    print >> stderr, "Randomizing queries"
    randomize_queries( qs )
    print >> stderr, "Writing queries"
    for q in qs:
        print " ".join( q )

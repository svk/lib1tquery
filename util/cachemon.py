from __future__ import with_statement

if __name__ == '__main__':
    import time
    import re
    delay = 10.0
    columns = [ "Cached", "MemFree", "Active", "Inactive" ]
    exs = [ (s, re.compile( "^%s:\s+(\w+) kB$" % s )) for s in columns ]
    with open( "cachemon.log", "a" ) as f:
        print >> f, "#", " ".join( [ "Time" ] + columns )
        while True:
            with open( "/proc/meminfo", "r" ) as g:
                rv = {}
                for line in g.readlines():
                    for name, expression in exs:
                        m = expression.match( line.strip() )
                        if m:
                            rv[ name ] = int( m.group(1) )
                numbers = [ time.time() ]
                for name in columns:
                    numbers.append( rv[name] )
                print >> f, " ".join( map( str, numbers ) )
            f.flush()
            time.sleep( delay )

if __name__ == '__main__':
    from sys import argv, stderr
    from random import shuffle
    for filename in argv[1:]:
        print >>stderr, "Scrambling", filename
        f = open( filename, "r" )
        g = open( "%s.scrambled" % filename, "w" )
        lines = f.readlines()
        f.close()
        shuffle( lines )
        for line in lines:
            print >>g, line
        g.close()

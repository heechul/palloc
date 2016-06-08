#! /usr/bin/env python
from itertools import *

import sys
import os
import getopt

def main():
    try:
        optlist, args = getopt.getopt(sys.argv[1:], 'h', ["help"])
    except getopt.GetoptError as err:
        print str(err)
        sys.exit(2)

    for opt, val in optlist:
        if opt in ("-h", "--help"):
            print "print possible combinations of given integer list"
            print "e.g.) $" + sys.argv[0] + " 13 14 15 16"
            sys.exit(0)
        else:
            assert False, "unhandled option"

    # print "ARGS: ", map(int, args)

    bits = map(int, args)
    for f in combinations(bits, 2):
        print f[0], f[1]
        

if __name__ == "__main__":
    main()

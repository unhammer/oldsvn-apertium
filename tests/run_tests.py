#!/usr/bin/env python3

import sys
import os
sys.path.append(os.path.realpath("."))

import unittest
import interchunk
import pretransfer
import tagger
import transfer

if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    failures = 0
    for module in [interchunk, pretransfer, tagger, transfer]:
        suite = unittest.TestLoader().loadTestsFromModule(module)
        res = unittest.TextTestRunner(verbosity = 2).run(suite)
        failures += len(res.failures)
    sys.exit(min(failures, 255))

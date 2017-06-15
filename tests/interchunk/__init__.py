#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import unittest

import itertools
from subprocess import Popen, PIPE, call, STDOUT
from tempfile import mkdtemp
from shutil import rmtree

import signal
class Alarm(Exception):
    pass

class InterchunkTest(unittest.TestCase):
    """Subclass and override inputs/expectedOutputs (and possibly other
stuff) to create new pretransfer tests."""

    inputs = [""]
    expectedOutputs = [""]
    expectedRetCodeFail = False

    def alarmHandler(self, signum, frame):
        raise Alarm

    def withTimeout(self, seconds, cmd, *args, **kwds):
        signal.signal(signal.SIGALRM, self.alarmHandler)
        signal.alarm(seconds)
        ret = cmd(*args, **kwds)
        signal.alarm(0)         # reset the alarm
        return ret



    def runTest(self):
        try:

            for inp, exp in zip(self.inputs, self.expectedOutputs):
                self.proc = Popen(['../apertium/apertium-interchunk', 'data/apertium-en-es.en-es.t2x', 'data/en-es.t2x.bin'], stdin = PIPE, stdout=PIPE, )
                self.assertEqual(self.proc.communicate(input=inp.decode('utf-8').encode('utf-8'))[0].strip().decode('utf-8').encode('utf-8')+"[][\n]", exp+"[][\n]")
                #strip and decode to remove '\n' at the end of output of communicate()
                #communicat() does the closing too.

            retCode = self.proc.poll()
            if self.expectedRetCodeFail:
                self.assertNotEqual(retCode, 0)
            else:
                self.assertEqual(retCode, 0)

        finally:
            pass

# mix of both inline and non-inline tags format.
class BlanksInterchunkTest(InterchunkTest):
    inputs =            ["[<div>]^Nom_adj<SN><UNDET><f><sg>{[{<i><b>}]^mente<n><3><4>$ [{<em>}]^bello<adj><3><4>$}$^punt<sent>{^.<sent>$}$",
                         "^Det_det_nom<SN><DET><GD><pl>{[{<i>}]^todo<predet><3><4>$ [{<i>}]^el<det><def><3><pl>$  [{<b>}]^estudiante<n><mf><4>$}$"
                        ]
    expectedOutputs =   ["[<div>]^Nom_adj<SN><UNDET><f><sg>{[{<i><b>}]^mente<n><3><4>$ [{<em>}]^bello<adj><3><4>$}$^punt<sent>{^.<sent>$}$",
                        "^Det_det_nom<SN><DET><m><pl>{[{<i>}]^todo<predet><3><4>$ [{<i>}]^el<det><def><3><pl>$  [{<b>}]^estudiante<n><mf><4>$}$"
                        ]

class BasicInterchunkTest(InterchunkTest):
    inputs =            ["^Nom_adj<SN><UNDET><GD><sg>{^Perro<n><3><4>$ ^blanco<adj><3><4>$}$",
                        "^Nom_adj<SN><UNDET><GD><sg>{^Perro<n><3><4>$^blanco<adj><3><4>$}$",
                        "^Det_det_nom<SN><DET><GD><pl>{^todo<predet><3><4>$ ^el<det><def><3><pl>$  ^estudiante<n><mf><4>$}$",
                        "^Det_det_nom<SN><DET><GD><pl>{^todo<predet><3><4>$🍰^el<det><def><3><pl>$  ^estudiante<n><mf><4>$}$"
                        ]
    expectedOutputs =   ["^Nom_adj<SN><UNDET><m><sg>{^Perro<n><3><4>$ ^blanco<adj><3><4>$}$",
                        "^Nom_adj<SN><UNDET><m><sg>{^Perro<n><3><4>$^blanco<adj><3><4>$}$",
                        "^Det_det_nom<SN><DET><m><pl>{^todo<predet><3><4>$ ^el<det><def><3><pl>$  ^estudiante<n><mf><4>$}$",
                        "^Det_det_nom<SN><DET><m><pl>{^todo<predet><3><4>$🍰^el<det><def><3><pl>$  ^estudiante<n><mf><4>$}$"
                        ]

class MultiInterchunkTest(InterchunkTest):
    inputs =            ["^Ger<SV><vblex><ger><PD><ND>{^ir<vblex><3>$}$ ^Nom<SN><UNDET><f><sg>{^casa<n><3><4>$}$",
                        "^SN<sg>{^cheese<n>$}$🍰^SN<sg>{^sale<n>$}$",
                        "[<div>]^Ger<SV><vblex><ger><PD><ND>{^correr<vblex><3>$}$ 🍰 ^Adj<SA><GD><ND>{[{<em>}]^rápido<adj><2><3>$}$"

                        ]
    expectedOutputs =   ["^Ger<SV><vblex><ger><PD><ND>{^ir<vblex><3>$}$ ^Nom<SN><UNDET><f><sg>{^casa<n><3><4>$}$",
                        "^SN<sg>{^cheese<n>$}$🍰^SN<sg>{^sale<n>$}$",
                        "[<div>]^Ger<SV><vblex><ger><PD><ND>{^correr<vblex><3>$}$ 🍰 ^Adj<SA><m><sg>{[{<em>}]^rápido<adj><2><3>$}$"
                        ]

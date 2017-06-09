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

class TransferTest(unittest.TestCase):
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
                self.proc = Popen(['../apertium/apertium-transfer', '-b','-t','data/apertium-en-es.en-es.t1x', 'data/en-es.t1x.bin'], stdin = PIPE, stdout=PIPE, )
                self.assertEqual(self.proc.communicate(input=inp.encode('utf-8'))[0].strip().decode('utf-8')+"[][\n]", exp+"[][\n]")
                #strip and decode to remove '\n' at the end of output of communicate()
                #communicat() does the closing too.

            retCode = self.proc.poll()
            if self.expectedRetCodeFail:
                self.assertNotEqual(retCode, 0)
            else:
                self.assertEqual(retCode, 0)

        finally:
            pass

class InlineBlankTransferTest(TransferTest):
    inputs =            ["[{<em>}]^beautiful<adj>/bello<adj>/bonito<adj>/lindo<adj>/hermoso<adj>$ [{<i><b>}]^mind<n><sg>/mente<n><f><sg>$^.<sent>/.<sent>$",
                         "^beautiful<adj>/bello<adj>/bonito<adj>/lindo<adj>/hermoso<adj>$ [{<i><b>}]^mind<n><sg>/mente<n><f><sg>$^.<sent>/.<sent>$",
                         "[{<b>}]^beautiful<adj>/bello<adj>/bonito<adj>/lindo<adj>/hermoso<adj>$ ^mind<n><sg>/mente<n><f><sg>$^.<sent>/.<sent>$",
                         "[{<i>}]^all<predet><sp>/todo<predet><GD><ND>$ [{<b>}]^student<n><pl>/estudiante<n><mf><pl>$",
                         "^all<predet><sp>/todo<predet><GD><ND>$ ^student<n><pl>/estudiante<n><mf><pl>$",
                         ]
    expectedOutputs =   ["^Nom_adj<SN><UNDET><f><sg>{[{<i><b>}]^mente<n><3><4>$ [{<em>}]^bello<adj><3><4>$}$^punt<sent>{^.<sent>$}$",
                         "^Nom_adj<SN><UNDET><f><sg>{[{<i><b>}]^mente<n><3><4>$ ^bello<adj><3><4>$}$^punt<sent>{^.<sent>$}$",
                         "^Nom_adj<SN><UNDET><f><sg>{^mente<n><3><4>$ [{<b>}]^bello<adj><3><4>$}$^punt<sent>{^.<sent>$}$",
                         "^Det_det_nom<SN><DET><GD><pl>{[{<i>}]^todo<predet><3><4>$ [{<i>}]^el<det><def><3><pl>$  [{<b>}]^estudiante<n><mf><4>$}$",
                         "^Det_det_nom<SN><DET><GD><pl>{^todo<predet><3><4>$ ^el<det><def><3><pl>$  ^estudiante<n><mf><4>$}$",
                         ]

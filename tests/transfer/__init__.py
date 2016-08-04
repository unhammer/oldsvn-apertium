#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import unittest

import itertools
from subprocess import Popen, PIPE, call
from tempfile import mkdtemp
from shutil import rmtree

import signal
class Alarm(Exception):
    pass

class PretransferTest(unittest.TestCase):
    """Subclass and override inputs/expectedOutputs (and possibly other
stuff) to create new pretransfer tests."""

    flags = ["-z"]
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

    def communicateFlush(self, string):
        self.proc.stdin.write(string.encode('utf-8'))
        self.proc.stdin.write(b'\0')
        self.proc.stdin.flush()

        output = []
        char = None
        try:
            char = self.withTimeout(2, self.proc.stdout.read, 1)
        except Alarm:
            pass
        while char and char != b'\0':
            output.append(char)
            try:
                char = self.withTimeout(2, self.proc.stdout.read, 1)
            except Alarm:
                break           # send what we got up till now

        return b"".join(output).decode('utf-8')

    def runTest(self):
        try:
            self.proc = Popen(["../apertium/apertium-transfer"] + self.flags,
                              stdin=PIPE,
                              stdout=PIPE,
                              stderr=PIPE)

            for inp, exp in zip(self.inputs, self.expectedOutputs):
                self.assertEqual(self.communicateFlush(inp+"[][\n]"),
                                 exp+"[][\n]")

            self.proc.communicate() # let it terminate
            self.proc.stdin.close()
            self.proc.stdout.close()
            self.proc.stderr.close()
            retCode = self.proc.poll()
            if self.expectedRetCodeFail:
                self.assertNotEqual(retCode, 0)
            else:
                self.assertEqual(retCode, 0)

        finally:
            pass


class BasictransferTest(TransferTest):
    inputs =          ["[2][{5}]^Post<n><sg>$ [{6}]^man<n><sg>$[]", "[{8}]^Just<adv>$ [{8}]^see# the point<vblex><inf>$[]"]
    expectedOutputs = ["[2]^Nom_pr_nom<SN><UNDET><m><sg>{[{6}]^hombre<n><3><4>$ [{5}]^de<pr>$ [{5}]^correo<n><m><sg>$[]}$", "^Adv<adv>{[{8}]^justo<adv>$[]}$ ^inf<SV><vblex><inf><PD><ND>{[{8}]^coger<vblex><3># la gracia$[]}$"]

class BasicinterchunkTest(TransferTest):
    inputs = ["[1]^gen-prep<pr>{^til<pr>$}$ [3]^n<n><m><sg><def><gen>{^bil<n><m><sg><def>$}$ [4]^n<n><nt><sg><ind>{[{2}]^problem<n><nt><sg><ind>$}$[]"]
    expectedOutputs = ["[4]^n<n><nt><sg><def>{[{2}]^problem<n><nt><sg><ind>$}$[1]^gen-prep<pr>{^til<pr>$}$ [3]^n<n><m><sg><def><gen>{^bil<n><m><sg><def>$}$[]"]

class BasicPostchunkTest(TransferTest):
    inputs = []
    expectedOutputs = []

class JoinGrouptransferTest(TransferTest):
    inputs =          ["[<div>]^a<vblex><pres>+c<po># b$",   "[<div>]^a<vblex><pres>+c<po>+d<po># b$"]
    expectedOutputs = ["[<div>]^a# b<vblex><pres>$ ^c<po>$", "[<div>]^a# b<vblex><pres>$ ^c<po>$ ^d<po>$"]


# Proposed inline blank format:
class InlineBlanktransferTest(TransferTest):
    inputs =          ["[{<i>}]^a<vblex><pres>+c<po># b$",          "[{<i>}]^a<vblex><pres>+c<po>+d<po># b$"]
    expectedOutputs = ["[{<i>}]^a# b<vblex><pres>$ [{<i>}]^c<po>$", "[{<i>}]^a# b<vblex><pres>$ [{<i>}]^c<po>$ [{<i>}]^d<po>$"]
    @unittest.expectedFailure
    def runTest(self):
        super().runTest(self)

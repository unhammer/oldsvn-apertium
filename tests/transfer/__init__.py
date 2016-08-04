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

class TransferTest(unittest.TestCase):
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
            self.proc = Popen(["../apertium/apertium-preprocess-transfer", "data/apertium-en-es.en-es.t1x", "data/en-es.t1x.bin"] + self.flags,
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


class BasicTransferTest(TransferTest):
    inputs =          ["[2][{5}]^Post<n><sg>$ [{6}]^man<n><sg>$[]",
                     "[1]^Prpers<prn><subj><p3><f><sg>$ ^can<vaux><past>$ ^not<adv>$ [{2}]^do<vbdo><pres>$[] ^the<det><def><sp>$[4] ^work<n><sg>$",
                     "[1][{2}]^Hello<ij>$ [{3}]^friend<n><sg>$ [4]^Do<vbdo><pres>$ [{5}]^prpers<prn><subj><p2><mf><sp>$ [{5}]^know<vblex><pres>$[] ^prpers<prn><obj><p1><mf><sg>$",
                     "[1][{2}]^Where<adv><itg>$ [{3}]^have<vblex><pres>$ [{3}]^prpers<prn><obj><p2><mf><sp>$[] ^be<vbser><pp>$",
                     "[1]^My<det><pos><sp>$ [2]^Uncle<n><sg>$ ^'s<gen>$[3] ^daughter<n><sg>$ [4]^be<vbser><pri><p3><sg>$ ^my<det><pos><sp>$ [5]^cousin<n><sg>$",
                     ]
    expectedOutputs = ["[2]^Nom_pr_nom<SN><UNDET><m><sg>{[{6}]^hombre<n><3><4>$ [{5}]^de<pr>$ [{5}]^correo<n><m><sg>$[]}$",
                     "[1]^Prnsubj<SN><tn><p3><f><sg>{^prpers<prn><2><p3><4><sg>$}$ ^mod<SV><vbmod><cni><PD><ND>{^poder<vbmod><3><4><5>$}$  ^adv<adv><NEG>{^no<adv>$}$  [4] ^det_nom<SN><DET><m><sg>{^el<det><def><3><4>$ ^trabajo<n><3><4>$}$",
                      "[1]^default<default>{[{2}]^Hola<ij>$[]}$ ^Nom<SN><UNDET><m><sg>{[{3}]^amigo<n><3><4>$}$ ^pro_verbcj<SV><vblex><pri><p1><sg>{^prpers<prn><pro><p1><mf><sg>$ [{5}]^saber<vblex><3><4><5>$}$",
                    "[1]^Adv<adv><itg>{[{2}]^dónde<adv><itg>$[]}$ ^pro_verbcj<SV><vblex><pri><p2><ND>{[{3}]^prpers<prn><pro><p2><mf><sg>$ [{3}]^tener<vblex><3><4><5>$[]}$ ^pp<SA><m><sg>{^ser<vbser><pp><2><3>$}$",
                     "[1] [2]^Det_nom<SN><DET><m><sg>{^mío<det><pos><mf><4>$ ^Tío<n><3><4>$}$ ^pr<GEN>{}$[3] ^nom<SN><UNDET><f><sg>{^hijo<n><3><4>$}$ [4]^be<Vcop><vbser><pri><p3><sg>{^ser<vbser><3><4><5>$}$ [5] ^det_nom<SN><DET><GD><sg>{^mío<det><pos><mf><4>$ ^primo<n><3><4>$}$"]
    @unittest.expectedFailure
    def runTest(self):
        super().runTest(self)

class InterchunkTransferTest(TransferTest):
    inputs =          ["[1]^Nom_pr_nom_pr_nom_pr_nom<SN><UNDET><f><sg>{[{2}]^olla<n><3><4>$ ^de<pr>$ ^té<n><m><sg>$[] ^de<pr>$ ^hombre<n><m><sg>$ ^de<pr>$ ^poste<n><m><sg>$}$", 
                        "[1]^Nom<SN><UNDET><f><sg>{^olla<n><3><4>$}$ [2]^Nom<SN><UNDET><f><sg>{^olla<n><3><4>$}$",
                        "[1]^gen-prep<pr>{^til<pr>$}$ [3]^n<n><m><sg><def><gen>{^bil<n><m><sg><def>$}$[4]^n<n><nt><sg><ind>{[{2}]^problem<n><nt><sg><ind>$}$[]"]
    expectedOutputs = ["[1]^Nom_pr_nom_pr_nom_pr_nom<SN><UNDET><f><sg>{[{2}]^olla<n><3><4>$ ^de<pr>$ ^té<n><m><sg>$[] ^de<pr>$ ^hombre<n><m><sg>$ ^de<pr>$ ^poste<n><m><sg>$}$",
                        "[1]^Nom<SN><UNDET><f><sg>{^olla<n><3><4>$}$ [2]^Nom<SN><UNDET><f><sg>{^olla<n><3><4>$}$"]
    @unittest.expectedFailure
    def runTest(self):
        super().runTest(self)


# Proposed inline blank format:
# class PostchunkTransferTest(TransferTest):
#     inputs =          ["[{<i>}]^a<vblex><pres>+c<po># b$",          "[{<i>}]^a<vblex><pres>+c<po>+d<po># b$"]
#     expectedOutputs = ["[{<i>}]^a# b<vblex><pres>$ [{<i>}]^c<po>$", "[{<i>}]^a# b<vblex><pres>$ [{<i>}]^c<po>$ [{<i>}]^d<po>$"]
    # @unittest.expectedFailure
    # def runTest(self):
    #     super().runTest(self)

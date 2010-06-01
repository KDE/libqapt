#! /usr/bin/env bash
$XGETTEXT `find utils/qapt-batch -name '*.cpp'` -o $podir/qapt-batch.pot
$XGETTEXT `find example -name '*.cpp'` -o $podir/qapttest.pot

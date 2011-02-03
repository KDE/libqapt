#! /usr/bin/env bash
$XGETTEXT `find utils/qapt-batch -name '*.cpp'` -o $podir/qaptbatch.pot
$XGETTEXT `find utils/qapt-gst-helper -name '*.cpp'` -o $podir/qapt-gst-helper.pot

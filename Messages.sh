#! /usr/bin/env bash
$XGETTEXT `find utils/qapt-batch -name '*.cpp'` -o $podir/qaptbatch.pot
$XGETTEXT `find utils/qapt-gst-helper -name '*.cpp'` -o $podir/qapt-gst-helper.pot
$XGETTEXT `find utils/qapt-deb-installer -name '*.cpp'` -o $podir/qapt-deb-installer.pot
$XGETTEXT `find utils/plasma-runner-installer -name '*.cpp'` -o $podir/plasma_runner_installer.pot
$EXTRACT_TR_STRINGS `find src -name '*.cpp'` -o $podir/libqapt.pot

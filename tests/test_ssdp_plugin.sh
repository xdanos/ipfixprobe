#!/bin/sh

test -z "$srcdir" && export srcdir=.

. $srcdir/test_plugin.sh

run_plugin_test ssdp "$pcap_dir/ssdp-sample.pcap"


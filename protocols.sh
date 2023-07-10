#!/bin/bash
protocols=$1
output_dir=$2
if [ -z "$protocols" ]; then
	echo "Usage: $0 <protocols> <output-dir>"
	exit 1
fi

for protocol in $protocols; do
	echo "Generating $protocol"
	# Look for the protocol first in ./protocols/
	# Then look for the protocol inside /usr/share/wayland-protocols
	if [ -f ./protocols/$protocol ]; then
		# Save the protocol into output_base, but remove the .xml extension
		output_base=$(basename $protocol .xml)
		wayland-scanner server-header ./protocols/$protocol $output_dir/$output_base-protocol.h
		wayland-scanner private-code ./protocols/$protocol $output_dir/$output_base-protocol.c
	else
		# Search for the protocol in {stable,unstable,staging}/$protocol
		# take the first XML file found
		protocol_file=$(find /usr/share/wayland-protocols/{stable,unstable,staging}/$protocol -name *.xml 2> /dev/null | head -n 1)
		if [ -z "$protocol_file" ]; then
			echo "Protocol $protocol not found"
			exit 1
		fi
		wayland-scanner server-header $protocol_file $output_dir/$protocol-protocol.h
		wayland-scanner private-code $protocol_file $output_dir/$protocol-protocol.c
	fi
done

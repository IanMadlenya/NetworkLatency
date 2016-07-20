=============================
Network Latency
=============================

An application to measure the latency between two network connections by sending
in identical packets and assigning each a time stamp.

Description
------------

This app is designed to read data from two different points in a network and
calculate the latency between them. Sample applications are provided that send
identical streams of ethernet packets over two of the network connections
available on an Isca board.

Each packet is identified by an ID number contained in the packet data, and
given a timestamp on arrival at the DFE. Timestamps are compared to calculate
the latency with microsecond accuracy.

The application's output consists of each calculated latency and its matching
ID number. This output is sent as a UDP stream on the connection used for the
second of the two incoming data streams.

Features
--------

*   ID numbers up to 19 bits are supported.
*   Sample C applications are provided to receive the output and to send
    in test packets, with output in both human-readable and CSV formats.
*   The latency output over UDP is given as a 32-bit unsigned integer in
    microseconds. The sample applications convert and display the returned
    latency as whole seconds with decimals.

Content
-------

The repo root directory contains the following items:

* APP
* LICENSE.txt

### APP

Directory containing project sources.

### LICENSE.txt

License of the project.


Information for compile
-----------------------

The project is provided with settings to build for simulation. To build for
hardware instead, change the following line in NetworkLatencyTypes.maxj from:

	declareParam(s_parameterTarget, Target.class, Target.DFE_SIM);

to:

	declareParam(s_parameterTarget, Target.class, Target.DFE);


Ensure the environment variables below are correctly set:

*   `MAXELEROSDIR`
*   `MAXCOMPILERDIR`

To compile the application, run:

	source project-utils/maxenv.sh
	source project-utils/config.sh
	cd bitstream
	ant
	ant NetworkLatency

The build process will echo the location of the .max file that is built. Copy
this file into the runtime directory, then run the build.py script. For example:

	cd runtime
	cp /local-scratch/kwallpe/maxdc_builds/15-07-16/NetworkLatency_ISCA_DFE_SIM/results/NetworkLatency.max .
	./build.py

To compile the sample send/receive applications:

	cd test
	./build.py

Running the application
-----------------------

Once compiled, change to the runtime directory and open three separate
terminals. In the first, run one of the following commands:

If built for simulation:

	export MAXELEROSDIR=$MAXCOMPILERDIR/lib/maxeleros-sim
	export LD_PRELOAD=$MAXELEROSDIR/lib/libmaxeleros.so:$LD_PRELOAD
	export SLIC_CONF="$SLIC_CONF;use_simulation=kwallpeSim"
	./build.py run_sim

If built for hardware:

	./networklatency

Once the message "Running. Press enter to exit." appears, go to the second
terminal and run:

	./test/receiver

Then go the third terminal and run:

	./test/sender

Each of these applications (networklatency, receiver, and sender) takes command
line arguments to change various settings, such as the IP addresses and netmasks
associated with the network connections, the number of packets to send,
artificial latency to add, etc. Use --help with each command to see a list of the
available arguments.

When finished testing, simply press Enter in the first terminal to stop the main
application, and halt the receiver application with Ctrl-C.

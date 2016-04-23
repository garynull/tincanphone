# Tin Can Phone

A simple C++ program I wrote to demonstrate peer-to-peer VOIP, for Linux and Windows.


# Precompiled binaries

Windows: http://garysinitsin.com/tincanphone.exe  
Linux: http://garysinitsin.com/tincanphone_0.1-1_amd64.deb


# How to use it

To make an outgoing call, input the IP address of another user running the program and press Call.

Your IP should be printed in the Tin Can Phone window after it starts up if UPnP is working on your network;
otherwise you may need to forward UDP port 56780 and look up your public IP yourself in order for incoming calls to work.


# Compiling

Tin Can Phone has 2 external dependencies:
[opus](https://www.opus-codec.org/) and [portaudio](http://portaudio.com/).
[miniupnpc](https://github.com/miniupnp/miniupnp) is also used, but is included in `miniupnpc.zip` for convenience, and should be compiled alongside Tin Can Phone.
Gtk3 is also required on Linux.

To compile on Linux, make sure the dev packages for the dependencies are installed, `unzip miniupnpc.zip`, then run `compile.sh`.
Note: on some distros you may need to `apt-get install libjack0` before `portaudio19-dev` to get the correct dev packages for compiling
(see [here](http://askubuntu.com/questions/526385/unable-to-install-libjack-dev)).

Otherwise, creating a project file for any IDE is pretty straightforward. Add the contents of either `src/Windows` or `src/Gtk` depending on your platform,
make sure to set up the above dependencies, and don't forget to define `MINIUPNP_STATICLIB`.


# Notes

The default port Tin Can Phone uses is UDP 56780, unless that port is already in use.
The GUI is implemented with Gtk3 on Linux and WinAPI on Windows.
Bug reports or fixes are welcome.

Although care was taken to create a usable application, some things were left out for simplicity:

* UPnP (or manual port-forwarding) is required instead of including code for other techniques such as proxying or hole-punching, which would also require a third party host.
* Network and audio I/O is all done in a synchronous fashion in a single thread. The GUI does run in a separate thread, though.
* IPv4 was assumed to make testing easier, but forward-compatible socket APIs were used.
* UPnP discovery is done every time the program starts instead of being saved so subsequent runs start up faster.


# License

MIT licensed open source; see LICENSE.txt. If you use this code for something cool, letting me know would be appreciated but it's not required.

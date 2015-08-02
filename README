pingfs - "True cloud storage"
	by Erik Ekman <erik@kryo.se>

pingfs is a filesystem where the data is stored only in the Internet itself,
as ICMP Echo packets (pings) travelling from you to remote servers and
back again.

It is implemented using raw sockets and FUSE, so superuser powers are required.
Linux is the only intended target OS, portability is not a goal.
Both IPv4 and IPv6 remote hosts are supported.

Compile by just running 'make'

How to start it:
- Create a textfile with hostname and IP addresses to target
- As root, run ./pingfs <filename> <mountpoint>
  It will resolve all hostnames, and then test each resolved address
  if it responds properly to a number of pings.
  Some statistics will be printed and then the filesystem will be mounted.
- Pingfs will stay in the foreground and print stats on packets and bytes
  each second.

How to stop it:
- Stop with ^C, and it should unmount itself.
- Otherwise unmount with fusermount -u <mountpoint>

Supported operations
- Creating/removing normal files
- Listing files
- Renaming files
- Reading/writing/truncating files
- Setting/getting file permissions

Unsupported operations
- Creating/removing directories
- Creating soft/hard links
- Timestamps (they are always 0)

Notes:
The performance is too low right now to handle LAN hosts, it will
lose data right away. Use pingfs with care.

License:

Copyright (c) 2013-2015 Erik Ekman <yarrick@kryo.se>

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

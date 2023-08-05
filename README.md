# pingfs - "True Cloud Storage"

[![License: ISC](https://img.shields.io/badge/License-ISC-blue.svg)](https://opensource.org/licenses/ISC) [![Linux Compatible](https://img.shields.io/badge/Linux-Compatible-brightgreen.svg)](https://shields.io/)

Developed by Erik Ekman (erik@kryo.se)

## Introduction

Pingfs is an innovative filesystem where the data is stored within the Internet itself, utilizing ICMP Echo packets (pings) that travel between your machine and remote servers.

## Compatibility & Requirements

Pingfs is designed for Linux, as portability is not an objective of this project. The filesystem is implemented using raw sockets and FUSE, hence superuser access is necessary. The application supports both IPv4 and IPv6 remote hosts.

## Building and Installation

To compile the application, simply use the `make` command in your terminal.

## Usage

### Getting Started:

1. Compile the project by running the `make` command.
2. Create a text file containing the hostnames and IP addresses to target.
3. As root, run the command `./pingfs <filename> <mountpoint>`. This will resolve all hostnames and test each resolved address for its responsiveness to pings. After this, the filesystem will be mounted.
4. Pingfs will remain active in the foreground and provide stats on packets and bytes each second.

### Halting the Process:

- Pingfs can be stopped with ^C command, and it should unmount itself.
- If this does not work, you can manually unmount with `fusermount -u <mountpoint>`.

## Features

### Supported Operations:

- Creation and removal of regular files.
- File listing.
- File renaming.
- Reading, writing, and truncating files.
- Setting and getting file permissions.

### Unsupported Operations:

- Creation and removal of directories.
- Creation of soft/hard links.
- Timestamps (they are always 0).

## Notes

The current performance of Pingfs is not sufficient to handle LAN hosts, as it may lose data immediately. Please use Pingfs with caution.

## License

Pingfs is released under the ISC License by Erik Ekman from 2013-2023.

Please be aware that this software is provided as is, and the author disclaims all warranties with regard to this software including all implied warranties of merchantability and fitness. In no event shall the author be liable for any special, direct, indirect, or consequential damages or any damages whatsoever resulting from loss of use, data or profits, whether in an action of contract, negligence or other tortious action, arising out of or in connection with the use or performance of this software.

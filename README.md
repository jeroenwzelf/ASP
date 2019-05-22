# ASP
_Audio Streaming Protocol - an UDP-based network protocol_

This repository contains an implementation (written in C, see _Implementation_) of a custom protocol that enables a server to stream audio to other clients over UDP/IP.
The specification of this custom protocol is available in _Specification_.

## Running the server and client
Build the programs using 
make

And run the server and client with:

  - ./server [file...]
  - ./client
respectively.

For setting custom options or with a usage, you can use:

  - ./server -h
  - ./client -h

which will print the usage of the program.
For example, running the simulation of a bad connection on the server side can be done with:
  - ./server -s [file...]


This project has been set up for a Computer Science assignment for the subject on Networking, Leiden University. See [the original assignment](http://liacs.leidenuniv.nl/~wijshoffhag/NETWERKEN2019/assignment2.pdf "2019 LIACS Networking assignment 2").

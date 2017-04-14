# rootkit
![](https://img.shields.io/badge/Development%20Status-In%20Progress-yellow.svg)

## About
This sample Linux rootkit is a loadable kernel module that hides itself from detection. It removes itself from `/proc/modules`
as well as `/sys/module`.

To communicate with the module send commands via `echo [cmd] > /dev/rk`.

## Commands
CMD | Description
------------ | -------------
`modhide` | Hides kernel module.
`modshow` | Reveals kernel module.
`phide` | Hides process with given PID.
`pshow` | Reveal process with given PID.

## Installation
Compile the module and load it with `sudo insmod rk.ko`. For now you must run `sudo chmod 666 /dev/rk` to set the appropriate
file permissions in order to give it commands. In the future I would like to have the rootkit do this programmatically.

## Notes
This module is being developed against Ubuntu 10.04 Server (Kernel v2.6.32). It has not been tested with any other releases.

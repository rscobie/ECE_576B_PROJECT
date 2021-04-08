# ECE_576B_PROJECT
An implementation of the Earliest Deadline Due (EDD) algorithm in FreeRTOS, with an example application of a simple health tracker.

## dependencies
git and a posix OS (tested on ubuntu 20.04)

## installation
install this repository using the following following:

```
git clone https://github.com/rscobie/ECE_576B_PROJECT
cd ECE_576B_PROJECT
git submodule init
git submodule update --init --recursive
```

## build
from inside the repository's root directory, call `make`

## run
`./build/edd_scheduler`
# Token_Bucket_Filter
The Linux kernel's network stack provides advanced network traffic controls. This repository attempts to emulate a classless qdisc (token bucket filter) to "shape" network traffic using multithreading in C. Four threads will be used during the program's execution. One thread will be for the packet arrival, one for the thread arrival, and one for each of the two servers being emulated. The program can run in one of two modes: deterministic or trace-driven. This project is intended for a Linux or macOS environment.

## To compile code
make qdisc

## To clean project and remove executables
make clean

## Usage on command line
usage: qdisc [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]

The default value (i.e., if it's not specified in a commandline option) for lambda is 1 (packets per second), the default value for mu is 0.35 (packets per second), the default value for r is 1.5 (tokens per second), the default value for B is 10 (tokens), the default value for P is 3 (tokens), and the default value for num is 20 (packets). B, P, and num must be positive integers with a maximum value of 2147483647 (0x7fffffff). lambda, mu, and r must be positive real numbers.

## Deterministic
In this mode, all inter-arrival times are equal to 1/lambda seconds, all packets require exactly P tokens, and all service times are equal to 1/mu seconds (all rounded to the nearest millisecond). If 1/lambda is greater than 10 seconds, an inter-arrival time of 10 seconds will be used. If 1/mu is greater than 10 seconds, a service time of 10 seconds will be used. 

## Trace-driven mode
In this mode, we will drive the emulation using a trace specification file (will be referred to as a "tsfile"). Each line in the trace file specifies the inter-arrival time of a packet, the number of tokens it need in order for it to be eligiable for transmission, and its service time.

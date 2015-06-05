# RAMSES ![RAMSES](http://image.spreadshirtmedia.net/image-server/v1/designs/11607520,width=100,height=100/pharaon.png)

RAMSES is the *Reversibility-based Agent Modeling and Simulation Environment with Speculation-support*.  
It is developed as a research project by the [High Performance and Dependable Computing Systems](http://www.dis.uniroma1.it/~hpdcs)
research group at Sapienza, University of Rome.


## Installation

RAMSES relies on autotools for installation. To generate the `configure` script, just launch `./autogen.sh` which should do all the job for you. You must have installed on your system `autoconf`, `automake`, and `libtoolize` for the process to complete correctly.

RAMSES relies on [hijacker](https://github.com/HPDCS/hijacker) to transparently generte undo code blocks, so it must be installed on the system.

Then, to compile and install RAMSES, issue the following commands:

`./configure`  
`make`  
`make install`

This will install `abmcc`, the RAMSES compiler on the system.

## Compiling a model

To compile a model, simply `cd` in the folder where the code is located. You can use `abmcc` to generate and link the prorgam to RAMSES, issuing a command like `abmcc *.c`.

## Writing a model compliant with RAMSES

We refer to [RAMSES wiki](https://github.com/HPDCS/RAMSES/wiki) for a discussion on the provided API and a tutorial on how to write a model to be run on top of RAMSES. We additionally refer to the `models` folder of the project for examples.

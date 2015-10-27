NS-3 
==========

This directory contains some released ns-3 version, along with 3rd party components necessary to support all optional ns-3 features, such as Python bindings and the Network Animator.This directory also contains the bake build tool, which allows access to several additional modules including the Network Simulation Cradle, Direct Code Execution environment, and click and openflow extensions for ns-3.

## Usage

Go to ns-3.19 folder

* Configure
```
$ ./waf configure --disable-python
```
* Run the simulator
```
$ ./waf --run "scratch/new_model_test1 –newModel=0"
```

scratch folder contains the code for test simulation


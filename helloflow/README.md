# helloflow
Examples to run the Foundation DB's Flow without dependecy on FDB. 

## BUILD

The build process is based on FoundationDB's build instrucution for both Linux and MacOS.

* Copy the **helloflow** folder to **foundationdb** folder
* Add **helloflow** the **foundationdb/Makefile** build target, add the following second line to Makefile.

```makefile
CPP_PROJECTS := flow fdbrpc fdbclient fdbbackup fdbserver fdbcli bindings/c bindings/java fdbmonitor bindings/flow/tester bindings/flow
CPP_PROJECTS +=  helloflow
```
* Under foundationdb folder, run **make targets**, *helloflow* should be listed as one of the targets.
* Run **make helloflow TLS_DISABLED=true**:  *bin/helloflow* should be built and ready to run.


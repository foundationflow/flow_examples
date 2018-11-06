# flowcli
Examples to run the Foundation DB's Flow without dependecy on FDB.  This example is based on the **fdbcli** and the dependency on **fdbclient** is taken out.

## BUILD

The build process is based on FoundationDB's build instrucution for both Linux and MacOS.

* Copy the **flowcli** folder to **foundationdb** folder
* Add **flowcli** the **foundationdb/Makefile** build target, add the following second line to Makefile.

```makefile
CPP_PROJECTS := flow fdbrpc fdbclient fdbbackup fdbserver fdbcli bindings/c bindings/java fdbmonitor bindings/flow/tester bindings/flow
CPP_PROJECTS += flowcli
```
* Under foundationdb folder, run **make targets**, *flowcli* should be listed as one of the targets.
* Run **make flowcli TLS_DISABLED=true**:  *bin/flowcli* should be built and ready to run, **exit** to quit from the CLI.



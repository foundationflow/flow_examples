# Helloflow
Examples to run the Foundation DB's Flow.  This example is based on the fdbcli and the dependency on FDB/fdbclient is taken out.

## BUILD

The build process is based on FoundationDB's build instrucution for both Linux and MacOS.

* Copy the **helloflow** folder to **foundationdb** folder
* Add **helloflow** the **foundationdb/Makefile** build target

```makefile
CPP_PROJECTS += helloflow
```
* Under foundationdb folder, run **make targets**, *helloflow* should be listed one of the targets.
* Run **make helloflow TLS_DISABLED=true**:  *bin/helloflow* should be built and ready to run.


**Note**:  current version still depends on fdbclient's *fdbclient/NativeAPI.h*.

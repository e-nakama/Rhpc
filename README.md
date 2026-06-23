# Rhpc

**Note: This package requires an MPI library.**

## Installation (Build)

### *nix-based OS

#### OpenMPI or MPICH2
To build a compressed source package (recommended):
```bash
apt install qpdf ghostscript
R CMD build --compact-vignettes=both Rhpc
R CMD INSTALL Rhpc_0.yy-yday.tar.gz
```

#### Other MPI implementations (example: Fujitsu MPI)
```bash
R CMD INSTALL Rhpc_0.yy-yday.tar.gz \
   --configure-args='--with-mpi-cflags=-I/opt/FJSVplang/include64/mpi/fujitsu \
                     --with-mpi-ldflags="-lmpi_f -lfjgmp64 -L/opt/FJSVpnple/lib -ljrm -L/opt/FJSVpnidt/lib -lfjidt -L/opt/FJSVplang/lib64 -lfj90i -lfj90f -lelf -lm -lpthread -lrt -ldl"'
```

### Windows (MS-MPI)
You need to set the `MSMPI` environment variables.

Example (checking MS-MPI environment variables):
```cmd
C:\Users\boofoo>set MSMPI
MSMPI_INC=C:\Program Files (x86)\Microsoft SDKs\MPI\Include\
MSMPI_LIB64=C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\
```

---

## Program Launch

### *nix-based OS

#### Via Rhpc shell (batch execution)
Use the `Rhpc` shell generated in the package root directory.
```bash
mpirun -n 4 ~/R/x86_64-unknown-linux-gnu-library/4.6/Rhpc/Rhpc CMD BATCH -q --no-save test.R
```

#### Dynamic launch inside an R session (MPI_Comm_Spawn)
If your environment supports it, you can dynamically spawn workers after starting R.
```r
library(Rhpc)
Rhpc_initialize()
cl <- Rhpc_getHandle(4)    # set number of workers
Rhpc_worker_call(cl, Sys.getpid)
# Example output:
# [[1]] [1] 10571
# [[2]] [1] 10572
# ...
Rhpc_finalize()
q("no")
```

### Microsoft Windows

First, install MS-MPI from the Microsoft Download Center.

#### Use in RGui
On Windows, Rhpc uses the `fakemaster` program so that MPI can be used from GUI environments like RGui. `Rhpc_getHandle()` runs `mpiexec` in the background and transfers the MPI environment to the current R process.

**Note:** Do not close the `mpiexec` or `fakemaster` command prompt windows that open during execution.

```r
library(Rhpc)
# Specify mpiexec arguments if needed
options(Rhpc.mpiexec="mpiexec -hosts=host1 -n 1")

Rhpc_initialize()
cl <- Rhpc_getHandle(3)
Rhpc_worker_call(cl, Sys.getpid)
Rhpc_finalize()
q("no")
```
* Note: Some versions of MS-MPI may have limitations for multi-node launch using `MPI_Comm_spawn` (confirmed to work on single node only).


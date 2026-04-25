\name{Rhpc-package}
\alias{Rhpc-package}
\alias{Rhpc_initialize}
\alias{Rhpc_finalize}
\alias{Rhpc_getHandle}
\alias{Rhpc_worker_call}
\alias{Rhpc_worker_shy}
\alias{Rhpc_worker_noback}
\alias{Rhpc_lapply}
\alias{Rhpc_lapplyLB}
\alias{Rhpc_sapply}
\alias{Rhpc_sapplyLB}
\alias{Rhpc_apply}
\alias{Rhpc_numberOfWorker}
\alias{Rhpc_Export}
\alias{Rhpc_EvalQ}
\alias{Rhpc_setupRNG}
\alias{Rhpc_enquote}
\alias{Rhpc_splitList}
\alias{Rhpc_serialize}
\alias{Rhpc_serialize_onlysize}
\alias{Rhpc_serialize_norealloc}
\alias{Rhpc_unserialize}

\usage{
Rhpc_initialize()
Rhpc_finalize()
Rhpc_getHandle(procs)
Rhpc_worker_call(cl, FUN, ..., usequote)
Rhpc_worker_noback(cl, FUN, ..., usequote)
Rhpc_lapply(cl, X, FUN, ...,usequote)
Rhpc_lapplyLB(cl, X, FUN, ..., usequote)
Rhpc_sapply(cl, X, FUN, ..., usequote, simplify, USE.NAMES)
Rhpc_sapplyLB(cl, X, FUN, ..., usequote, simplify, USE.NAMES)
Rhpc_apply(cl, X, MARGIN, FUN, ..., usequote)
Rhpc_numberOfWorker(cl)
Rhpc_Export(cl, variableNames, usequote, pos, envir)
Rhpc_EvalQ(cl, expr, usequote, envir)
Rhpc_setupRNG(cl, iseed)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{procs}{Specify the number of processes when performing dynamic
    process generation (use the MPI_Comm_spawn), only static generate
    if blank}
  \item{X}{vector or list.
    Divided into smaller vectors according to the number of workers, and
    distributed to workers when the function is first executed. One-sided
    communication is used asynchronously
  }
  \item{FUN}{string of function name or function object.
    Function name or string (string expresses function name)
    Distributed by collective communication at first, then they are not
    sent again
  }
  \item{MARGIN}{a vector giving the subscripts which the function will
    be applied over.  E.g., for a matrix \code{1} indicates rows,
    \code{2} indicates columns, \code{c(1, 2)} indicates rows and
    columns. Where \code{X} has named dimnames, it can be a character
    vector selecting dimension names.
  }
  \item{simplify}{logical or character string; should the result be
    simplified to a vector, matrix or higher dimensional array if
    possible?  For \code{sapply} it must be named and not abbreviated.
    The default value, \code{TRUE}, returns a vector or matrix if appropriate,
    whereas if \code{simplify = "array"} the result may be an
    \code{\link{array}} of \dQuote{rank}
    (\eqn{=}\code{length(dim(.))}) one higher than the result
    of \code{FUN(X[[i]])}.
  }
  \item{USE.NAMES}{logical; if \code{TRUE} and if \code{X} is character,
    use \code{X} as \code{\link{names}} for the result unless it had names
    already.  Since this argument follows \code{\dots} its name cannot
    be abbreviated.
  }
  \item{...}{arguments.
    Distributed by collective communication at first, then they are not
    sent again
  }
  \item{usequote}{user can choose usequpte=FALSE for performance improvement}
  \item{variableNames}{string vector} 
  \item{iseed}{random number seed}
  \item{expr}{expression to evaluate}
  \item{pos}{default is current environment position}
  \item{envir}{where to look for the object}
}
\docType{package}
\title{
  permits *apply() style dispatch for HPC
}
\description{
 Rhpc_lapply, Rhpc_lapplyLB and Rhpc_worker_call using MPI provides 
 better HPC environment on R(works fast on HPC).
 and this package supports long vector, can deal with slightly big data.
}
\details{
  Rhpc batch wrapper in package directory if you use mpirun.

  This package sets \code{"Rhpc.mpi.c.comm"},\code{"Rhpc.mpi.f.comm"},
  \code{"Rhpc.mpi.rank"} and \code{"Rhpc.mpi.procs"}
  can get attribute of MPI in getOption().  

  \url{https://prs.ism.ac.jp/~nakama/Rhpc/}
}
\author{
  Junji NAKANO <nakanoj@ism.ac.jp> and Ei-ji Nakama<nakama@com-one.com>
 
  Maintainer: Ei-ji Nakama <nakama@com-one.com>
}
\keyword{utilities}
\examples{
\dontrun{
# use Rhpc command
Rhpc_initialize()

# If you start the process dynamically on Windows, the dos window of
# mpiexec will be created, but you should not close this window.
cl <- Rhpc_getHandle(5) # ommit number of worker if use Rhpc batch

# Rhpc set to options
opstr=list("Rhpc.mpi.rank","Rhpc.mpi.procs","Rhpc.mpi.c.comm","Rhpc.mpi.f.comm")
do.call("options",opstr)
Rhpc_worker_call(cl, "do.call","options", opstr)
# warning! : pointer not export, worker Rhpc.mpi.c.comm is (nil) on master.

Rhpc_worker_call(cl, Sys.getpid)
Rhpc_lapply(cl,1:10000,sqrt)
Rhpc_lapplyLB(cl,1:10000,sqrt)
Rhpc_sapply(cl,1:10000,sqrt)
Rhpc_sapplyLB(cl,1:10000,sqrt)

df<-data.frame(a=1:4,b=5:8)
Rhpc_apply(cl,df,1,max)
Rhpc_apply(cl,df,2,max)

Rhpc_finalize()
#
# command line example
#
# mpirun -n 4 Rhpc CMD BATCH --no-save `script file for exemplified above'
#
}
}

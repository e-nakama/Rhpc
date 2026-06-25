\name{Rhpc-package}
\alias{Rhpc-package}
\alias{Rhpc_gethandle}
\alias{Rhpc_mpi_initialize}
\alias{Rhpc_mpi_finalize}
\alias{Rhpc_mpi_lapply_LB}
\alias{Rhpc_mpi_lapply_seq}
\alias{Rhpc_mpi_worker_call}
\alias{Rhpc_number_of_worker}

\docType{package}
\title{Rhpc: MPI-based apply-style parallel computing for R}
\description{Provides MPI-aware apply-style functions and worker utilities for high-performance computing in R.}
\details{
  The Rhpc package offers MPI-aware versions of common apply-style functions and helper utilities for distributed computation.
  It supports both static worker configuration and dynamic process generation.

  Main functions:
  \describe{
    \item{\code{\link{Rhpc_initialize}}}{Initialize MPI environment}
    \item{\code{\link{Rhpc_getHandle}}}{Get communicator handle to workers}
    \item{\code{\link{Rhpc_lapply}}, \code{\link{Rhpc_lapplyLB}}}{Parallel lapply variants}
    \item{\code{\link{Rhpc_sapply}}, \code{\link{Rhpc_sapplyLB}}}{Parallel sapply variants}
    \item{\code{\link{Rhpc_apply}}}{Parallel apply for arrays/matrices}
    \item{\code{\link{Rhpc_worker_call}}}{Call functions on workers}
    \item{\code{\link{Rhpc_Export}}}{Export variables to workers}
    \item{\code{\link{Rhpc_setupRNG}}}{Setup random number generators}
    \item{\code{\link{Rhpc_finalize}}}{Clean up MPI environment}
  }

  MPI-related options are exposed through \code{getOption()}: \code{Rhpc.mpi.c.comm}, \code{Rhpc.mpi.f.comm}, \code{Rhpc.mpi.rank}, and \code{Rhpc.mpi.procs}.

  See \url{https://github.com/e-nakama/Rhpc} for more information and examples.
}
\author{Ei-ji Nakama <nakama@com-one.com> and Junji NAKANO <nakanoj@ism.ac.jp>

Maintainer: Ei-ji Nakama <nakama@com-one.com>}
\keyword{package}
\examples{
## Simple serialization example (no MPI required)
data_to_serialize <- list(a = 1:5, b = c("x", "y", "z"))
serialized <- Rhpc_serialize(data_to_serialize)
deserialized <- Rhpc_unserialize(serialized)
stopifnot(identical(data_to_serialize, deserialized))

\donttest{
#Rhpc_initialize()
#cl <- Rhpc_getHandle(3)
#result <- Rhpc_lapply(cl, 1:100, sqrt)
#Rhpc_finalize()
}
}

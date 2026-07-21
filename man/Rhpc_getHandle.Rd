\name{Rhpc_getHandle}
\alias{Rhpc_getHandle}
\title{Get MPI communicator handle}
\description{Obtains a handle to the MPI communicator for worker processes.}
\usage{
Rhpc_getHandle(procs = NA)
}
\arguments{
  \item{procs}{number of processes to create via dynamic process generation. If \code{NA} (default),
    uses static configuration.}
}
\details{
  This function returns an external pointer to the MPI communicator, which is used in all subsequent
  Rhpc function calls. It also initializes the random number generation stream for workers.
  
  If \code{procs} is specified, dynamic process generation is used (MPI_Comm_spawn).
  If \code{procs} is \code{NA}, static worker configuration is assumed.
}
\value{
  An external pointer to the MPI communicator handle, which can be passed to other Rhpc functions.
}
\seealso{
  \code{\link{Rhpc_initialize}}, \code{\link{Rhpc_numberOfWorker}}, \code{\link{Rhpc_finalize}}
}
\keyword{utilities}

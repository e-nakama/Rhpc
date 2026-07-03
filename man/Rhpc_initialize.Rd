\name{Rhpc_initialize}
\alias{Rhpc_initialize}
\title{Initialize MPI environment}
\description{Initializes the MPI environment for Rhpc.}
\usage{
Rhpc_initialize()
}
\details{
  This function must be called before any other Rhpc functions to initialize the MPI communicator.
  It sets up the MPI environment and prepares the system for distributed computation.
}
\value{
  Returns invisibly. MPI initialization results are stored internally.
}
\seealso{
  \code{\link{Rhpc_finalize}}, \code{\link{Rhpc_getHandle}}
}
\keyword{utilities}

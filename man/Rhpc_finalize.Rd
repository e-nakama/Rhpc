\name{Rhpc_finalize}
\alias{Rhpc_finalize}
\title{Finalize MPI environment}
\description{Finalizes the MPI environment and terminates all worker processes.}
\usage{
Rhpc_finalize()
}
\details{
  This function should be called at the end of an Rhpc session to properly shut down the MPI environment and clean up all resources.
}
\value{
  Returns invisibly. Resources are cleaned up.
}
\seealso{
  \code{\link{Rhpc_initialize}}, \code{\link{Rhpc_getHandle}}
}
\examples{
\donttest{
#Rhpc_initialize()
# Use Rhpc functions
#Rhpc_finalize()
}
}
\keyword{utilities}

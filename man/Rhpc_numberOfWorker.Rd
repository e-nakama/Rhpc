\name{Rhpc_numberOfWorker}
\alias{Rhpc_numberOfWorker}
\title{Get number of worker processes}
\description{Returns the number of active worker processes.}
\usage{
Rhpc_numberOfWorker(cl)
}
\arguments{
  \item{cl}{external pointer to MPI communicator (obtained from \code{\link{Rhpc_getHandle}})}
}
\value{
  An integer giving the number of worker processes.
}
\seealso{
  \code{\link{Rhpc_getHandle}}
}
\examples{
\donttest{
#Rhpc_initialize()
#cl <- Rhpc_getHandle(3)
#n_workers <- Rhpc_numberOfWorker(cl)
#print(n_workers)  # Should print 5
#Rhpc_finalize()
}
}
\keyword{utilities}

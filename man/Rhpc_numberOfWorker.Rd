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
\keyword{utilities}

\name{Rhpc_worker_noback}
\alias{Rhpc_worker_noback}
\title{Call a function on worker processes without collecting results}
\description{Executes a function on all worker processes without waiting for or collecting results.}
\usage{
Rhpc_worker_noback(cl, FUN, ..., usequote = TRUE)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{FUN}{function or string naming the function to be called on workers}
  \item{...}{arguments to pass to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
}
\details{
  This function sends a function call to all worker processes but does not wait for results.
  It is useful for operations like setting options or initializing worker state where return values are not needed.
}
\value{
  Returns invisibly. No results are collected from workers.
}
\seealso{
  \code{\link{Rhpc_worker_call}}, \code{\link{Rhpc_Export}}
}
\examples{
\dontrun{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(３)
Rhpc_worker_noback(cl, "options", list("digits", 15))
Rhpc_finalize()
}
}
\keyword{utilities}

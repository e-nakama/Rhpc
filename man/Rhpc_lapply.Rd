\name{Rhpc_lapply}
\alias{Rhpc_lapply}
\title{Parallel lapply using MPI}
\description{MPI-based version of \code{lapply} that distributes work across worker processes sequentially.}
\usage{
Rhpc_lapply(cl, X, FUN, ..., usequote = TRUE)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{X}{vector or list to be processed}
  \item{FUN}{function to apply to each element of \code{X}}
  \item{...}{additional arguments passed to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
}
\details{
  This function distributes elements of \code{X} sequentially to worker processes.
  The input \code{X} is divided among workers and each worker processes its portion in order.
  
  Results are collected and returned in a list maintaining the original order.
}
\value{
  A list with results from applying \code{FUN} to each element of \code{X}.
}
\seealso{
  \code{\link{Rhpc_lapplyLB}}, \code{\link{Rhpc_sapply}}, \code{\link{Rhpc_apply}}
}
\examples{
\dontrun{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(5)
result <- Rhpc_lapply(cl, 1:1000, sqrt)
Rhpc_finalize()
}
}
\keyword{utilities}

\name{Rhpc_lapplyLB}
\alias{Rhpc_lapplyLB}
\title{Parallel lapply with load balancing}
\description{MPI-based version of \code{lapply} with load balancing for better performance on heterogeneous systems.}
\usage{
Rhpc_lapplyLB(cl, X, FUN, ...,
              usequote = ifelse(is.logical(getOption("Rhpc.usequote")),
                                getOption("Rhpc.usequote"), TRUE))
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{X}{vector or list to be processed}
  \item{FUN}{function to apply to each element of \code{X}}
  \item{...}{additional arguments passed to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
}
\details{
  This function distributes work to workers using a load-balancing approach.
  Workers receive tasks dynamically as they become available, which can provide better performance
  when task execution times vary significantly.
  
  Results are collected and returned in a list maintaining the original order.
}
\value{
  A list with results from applying \code{FUN} to each element of \code{X}.
}
\seealso{
  \code{\link{Rhpc_lapply}}, \code{\link{Rhpc_sapplyLB}}, \code{\link{Rhpc_apply}}
}
\examples{
\donttest{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(3)
result <- Rhpc_lapplyLB(cl, 1:1000, sqrt)
Rhpc_finalize()
}
}
\keyword{utilities}

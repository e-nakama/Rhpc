\name{Rhpc_sapplyLB}
\alias{Rhpc_sapplyLB}
\title{Parallel sapply with load balancing}
\description{MPI-based version of \code{sapply} with load balancing for better performance.}
\usage{
Rhpc_sapplyLB(cl = NULL, X, FUN, ...,
              usequote = ifelse(is.logical(getOption("Rhpc.usequote")),
                                getOption("Rhpc.usequote"), TRUE),
              simplify = TRUE,
              USE.NAMES = TRUE)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{X}{vector or list to be processed}
  \item{FUN}{function to apply to each element of \code{X}}
  \item{...}{additional arguments passed to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
  \item{simplify}{logical or character string; if possible, simplify the result}
  \item{USE.NAMES}{logical; if \code{TRUE} and \code{X} is character, use \code{X} as names}
}
\details{
  This function combines load-balanced distribution with result simplification.
  Workers receive tasks dynamically as they become available, and results are simplified when possible.
}
\value{
  A simplified version of the result (vector, matrix, array, or list) depending on \code{simplify}.
}
\seealso{
  \code{\link{Rhpc_sapply}}, \code{\link{Rhpc_lapplyLB}}, \code{\link{Rhpc_apply}}
}
\examples{
\donttest{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(3)
result <- Rhpc_sapplyLB(cl, 1:100, sqrt)
Rhpc_finalize()
}
}
\keyword{utilities}

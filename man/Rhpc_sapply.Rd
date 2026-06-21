\name{Rhpc_sapply}
\alias{Rhpc_sapply}
\title{Parallel sapply using MPI}
\description{MPI-based version of \code{sapply} that distributes work across worker processes.}
\usage{
Rhpc_sapply(cl = NULL, X, FUN, ..., usequote = TRUE, simplify = TRUE, USE.NAMES = TRUE)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{X}{vector or list to be processed}
  \item{FUN}{function to apply to each element of \code{X}}
  \item{...}{additional arguments passed to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
  \item{simplify}{logical or character string; if possible, simplify the result to a vector or matrix}
  \item{USE.NAMES}{logical; if \code{TRUE} and \code{X} is character, use \code{X} as names for the result}
}
\details{
  This function combines the functionality of \code{Rhpc_lapply} with automatic simplification of results.
  When possible, results are simplified to a vector or matrix form rather than remaining as a list.
}
\value{
  A simplified version of the result (vector, matrix, or array) or list depending on \code{simplify} parameter.
}
\seealso{
  \code{\link{Rhpc_sapplyLB}}, \code{\link{Rhpc_lapply}}, \code{\link{Rhpc_apply}}
}
\examples{
\dontrun{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(３)
result <- Rhpc_sapply(cl, 1:100, sqrt)
Rhpc_finalize()
}
}
\keyword{utilities}

\name{Rhpc_apply}
\alias{Rhpc_apply}
\title{Parallel apply using MPI}
\description{MPI-based version of \code{apply} for arrays and matrices.}
\usage{
Rhpc_apply(cl = NULL, X, MARGIN, FUN, ..., usequote = TRUE)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{X}{array or matrix}
  \item{MARGIN}{vector giving the subscripts which the function will be applied over.
    \code{1} indicates rows, \code{2} indicates columns, \code{c(1, 2)} indicates rows and columns}
  \item{FUN}{function to apply}
  \item{...}{additional arguments passed to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
}
\details{
  This function applies \code{FUN} to array margins specified by \code{MARGIN},
  distributing the work across MPI workers.
  
  If \code{X} has dimension subscripts not in \code{MARGIN}, those are used as the margin dimensions
  for applying the function.
}
\value{
  Array or matrix of results from applying \code{FUN} to the specified margins.
}
\seealso{
  \code{\link{Rhpc_lapply}}, \code{\link{Rhpc_sapply}}
}
\examples{
\dontrun{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(３)
df <- data.frame(a = 1:4, b = 5:8)
result <- Rhpc_apply(cl, df, 1, max)  # Max of each row
Rhpc_finalize()
}
}
\keyword{utilities}

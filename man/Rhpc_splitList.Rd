\name{Rhpc_splitList}
\alias{Rhpc_splitList}
\title{Split a list into chunks}
\description{Divides a list or vector into approximately equal-sized chunks for distribution to workers.}
\usage{
Rhpc_splitList(var, num)
}
\arguments{
  \item{var}{vector or list to be split}
  \item{num}{number of chunks to create (typically equal to the number of workers)}
}
\details{
  This function divides \code{var} into \code{num} roughly equal-sized chunks.
  It is useful for manually distributing work to workers when fine-grained control is needed.
  
  This is primarily an internal utility; the apply-style functions handle splitting automatically.
}
\value{
  A list of length \code{num}, each element containing a portion of \code{var}.
}
\seealso{
  \code{\link{Rhpc_lapply}}, \code{\link{Rhpc_numberOfWorker}}
}
\keyword{utilities}

\name{Rhpc_worker_call}
\alias{Rhpc_worker_call}
\title{Call a function on worker processes}
\description{Executes a function on all worker processes and collects the results.}
\usage{
Rhpc_worker_call(cl, FUN, ...,
                 usequote = ifelse(is.logical(getOption("Rhpc.usequote")),
                                   getOption("Rhpc.usequote"), TRUE))
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{FUN}{function or string naming the function to be called on workers}
  \item{...}{arguments to pass to \code{FUN}}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
}
\details{
  This function broadcasts a function call to all worker processes and waits for results.
  Results from all workers are collected and returned.
  
  The function or its name is sent to workers via collective communication only on the first call;
  subsequent calls with the same function reuse the cached version.
}
\value{
  Results collected from all worker processes.
}
\seealso{
  \code{\link{Rhpc_worker_noback}}, \code{\link{Rhpc_lapply}}, \code{\link{Rhpc_EvalQ}}
}
\examples{
\dontrun{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(5)
result <- Rhpc_worker_call(cl, Sys.getpid)
print(result)
Rhpc_finalize()
}
}
\keyword{utilities}

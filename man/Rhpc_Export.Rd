\name{Rhpc_Export}
\alias{Rhpc_Export}
\title{Export variables to worker processes}
\description{Sends R objects from the master process to all worker processes.}
\usage{
Rhpc_Export(cl, variableNames,
            usequote = ifelse(is.logical(getOption("Rhpc.usequote")),
                              getOption("Rhpc.usequote"), TRUE),
            pos = 1,
            envir = as.environment(pos))
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{variableNames}{character vector of object names to export}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
  \item{pos}{environment position from which to retrieve objects (default: global environment)}
  \item{envir}{environment from which to retrieve objects (default: environment at position \code{pos})}
}
\details{
  This function retrieves objects from the master's environment and sends them to all worker processes,
  making them available in the workers' global environment.
  
  Objects are sent via collective communication.
}
\value{
  Returns invisibly. Objects are assigned in worker environments.
}
\seealso{
  \code{\link{Rhpc_worker_call}}, \code{\link{Rhpc_EvalQ}}
}
\examples{
\donttest{
## Need MPI Cluster
Rhpc_initialize()
cl <- Rhpc_getHandle(3)
my_data <- 1:100
Rhpc_Export(cl, "my_data")
# Now workers can use my_data
Rhpc_finalize()
}
}
\keyword{utilities}

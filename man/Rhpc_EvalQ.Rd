\name{Rhpc_EvalQ}
\alias{Rhpc_EvalQ}
\title{Evaluate an expression on worker processes}
\description{Evaluates an R expression on all worker processes.}
\usage{
Rhpc_EvalQ(cl, expr,
           usequote = ifelse(is.logical(getOption("Rhpc.usequote")),
                             getOption("Rhpc.usequote"), TRUE),
           envir = .GlobalEnv)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{expr}{expression to evaluate on workers}
  \item{usequote}{logical; if \code{FALSE}, quote handling is skipped for performance improvement}
  \item{envir}{environment in which to evaluate the expression on workers (default: global environment)}
}
\details{
  This function evaluates the given expression on all worker processes and returns the results.
  It is useful for initialization tasks or querying worker state.
}
\value{
  Results of evaluating the expression on all workers.
}
\seealso{
  \code{\link{Rhpc_worker_call}}, \code{\link{Rhpc_Export}}
}
\keyword{utilities}

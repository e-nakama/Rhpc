\name{Rhpc_enquote}
\alias{Rhpc_enquote}
\title{Quote arguments for remote evaluation}
\description{Quotes arguments for use in remote function calls on worker processes.}
\usage{
Rhpc_enquote(...)
}
\arguments{
  \item{...}{arguments to be quoted}
}
\details{
  This function quotes its arguments to prevent premature evaluation in the master process.
  The quoted expressions are then evaluated on the worker processes where they have access
  to the worker's data and environment.
  
  This is primarily an internal utility; users typically do not need to call it directly.
}
\value{
  A quoted representation of the arguments suitable for remote evaluation.
}
\seealso{
  \code{\link{Rhpc_worker_call}}, \code{\link{Rhpc_EvalQ}}
}
\keyword{utilities}

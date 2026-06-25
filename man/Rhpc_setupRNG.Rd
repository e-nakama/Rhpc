\name{Rhpc_setupRNG}
\alias{Rhpc_setupRNG}
\title{Setup random number generators for worker processes}
\description{Initializes independent random number streams for each worker process using L'Ecuyer-CMRG algorithm.}
\usage{
Rhpc_setupRNG(cl, iseed = NULL)
}
\arguments{
  \item{cl}{external pointer to MPI communicator}
  \item{iseed}{optional integer seed for reproducibility. If \code{NULL}, uses current .Random.seed}
}
\details{
  This function sets up independent random number streams for each worker to ensure they produce
  different but reproducible random sequences. It uses the L'Ecuyer-CMRG (Combined Multiple Recursive Generator)
  algorithm.
  
  The master process's original .Random.seed is saved to \code{options("Rhpc.oldseed")} but is NOT
  automatically restored due to CRAN policy. Users should manually restore if needed:
  \code{assign(".Random.seed", getOption("Rhpc.oldseed"), envir = globalenv())}
}
\value{
  Returns invisibly the original .Random.seed that was saved.
}
\seealso{
  \code{\link{Rhpc_getHandle}}, \code{\link{Rhpc_lapply}}
}
\examples{
\donttest{
#Rhpc_initialize()
#cl <- Rhpc_getHandle(3)
#Rhpc_setupRNG(cl, iseed = 123)  # Set seed for reproducibility
## Now workers have independent random streams
#Rhpc_finalize()
}
}
\keyword{utilities}

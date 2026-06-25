\name{Rhpc_serialize}
\alias{Rhpc_serialize}
\alias{Rhpc_serialize_onlysize}
\alias{Rhpc_serialize_norealloc}
\alias{Rhpc_unserialize}
\title{Serialize and unserialize R objects}
\description{Serialize R objects for transmission or storage, and unserialize them back.}
\usage{
Rhpc_serialize(obj)
Rhpc_serialize_onlysize(obj)
Rhpc_serialize_norealloc(obj)
Rhpc_unserialize(obj)
}
\arguments{
  \item{obj}{R object to serialize, or serialized object to unserialize}
}
\details{
  These functions provide low-level serialization utilities for handling R objects:
  
  \describe{
    \item{\code{Rhpc_serialize}}{Fully serializes an object to raw bytes, allocating necessary memory}
    \item{\code{Rhpc_serialize_onlysize}}{Returns only the size needed for serialization without actual serialization}
    \item{\code{Rhpc_serialize_norealloc}}{Serializes without reallocating memory (for efficiency)}
    \item{\code{Rhpc_unserialize}}{Reconstructs an R object from its serialized form}
  }
  
  These functions are primarily intended for internal Rhpc use but may be useful for debugging or advanced users.
}
\value{
  For \code{Rhpc_serialize}: raw vector containing serialized data
  
  For \code{Rhpc_serialize_onlysize}: integer giving the size in bytes
  
  For \code{Rhpc_serialize_norealloc}: raw vector containing serialized data
  
  For \code{Rhpc_unserialize}: original R object
}
\examples{
## Serialization example (executable without MPI)
data_to_serialize <- list(a = 1:5, b = c("x", "y", "z"))
serialized <- Rhpc_serialize(data_to_serialize)
deserialized <- Rhpc_unserialize(serialized)
stopifnot(identical(data_to_serialize, deserialized))
}
\keyword{utilities}


Rhpc_serialize <- function(obj)
{
  .Call("Rhpc_serialize", obj, PACKAGE="Rhpc")
}

Rhpc_serialize_onlysize <- function(obj)
{
  .Call("Rhpc_serialize_onlysize", obj, PACKAGE="Rhpc")
}

Rhpc_serialize_norealloc <- function(obj)
{
  .Call("Rhpc_serialize_norealloc", obj, PACKAGE="Rhpc")
}

Rhpc_unserialize <- function(obj)
{
  .Call("Rhpc_unserialize", obj, PACKAGE="Rhpc")
}


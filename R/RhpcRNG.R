# from parallel:::clusterSetRNGStream(GPL-2)
# Modified: removed direct assign/rm of .Random.seed on the master (CRAN policy).
Rhpc_setupRNG <- function(cl, iseed = NULL)
{
    oldseed <-
        if (exists(".Random.seed", envir = globalenv(), inherits = FALSE))
            get(".Random.seed", envir = globalenv(), inherits = FALSE)
        else NULL
    
    options(Rhpc.oldseed = oldseed)
    
    oldkind <- RNGkind()
    on.exit(do.call(RNGkind, as.list(oldkind)))

    RNGkind("L'Ecuyer-CMRG")
    if (!is.null(iseed)) set.seed(iseed)
    nc <- Rhpc_numberOfWorker(cl)
    seeds <- vector("list", nc)
    seeds[[1L]] <- .Random.seed
    for (i in seq_len(nc - 1L))
        seeds[[i + 1L]] <- parallel::nextRNGStream(seeds[[i]])

    message("Note: Rhpc_setupRNG saves .Random.seed to options(\"Rhpc.oldseed\") but does not restore it on the master process.\n",
            "To restore manually: assign(\".Random.seed\", getOption(\"Rhpc.oldseed\"), envir = globalenv())")
    Rhpc_lapply(cl, seeds, function(seed) assign(".Random.seed", seed, envir = globalenv()))
    invisible(oldseed)
}

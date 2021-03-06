% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/calcGeneGSStat.R
\name{computeGeneGSProb}
\alias{computeGeneGSProb}
\title{Compute Gene Probability}
\usage{
computeGeneGSProb(Amean, Asd, GSGenes, Pw = rep(1, ncol(Amean)),
  numPerm = 500, PwNull = FALSE)
}
\arguments{
\item{Amean}{A matrix mean values}

\item{Asd}{A matrix standard deviations}

\item{GSGenes}{data.frame or list with gene sets}

\item{Pw}{weight on genes}

\item{numPerm}{number of permutations for null}

\item{PwNull}{- logical indicating gene adjustment}
}
\value{
A vector of length GSGenes containing the p-values of set membership
for each gene containined in the set specified in GSGenes.
}
\description{
Compute Gene Probability
}
\details{
Computes the p-value for gene set membership using the CoGAPS-based
statistics developed in Fertig et al. (2012).  This statistic refines set
membership for each candidate gene in a set specified in \code{GSGenes} by
comparing the inferred activity of that gene to the average activity of the
set.
}
\examples{
data('SimpSim')
computeGeneGSProb(SimpSim.result$Amean, SimpSim.result$Asd, GSGenes=GSets[[1]],
numPerm=500)
}


% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/plotP.R
\name{plotP}
\alias{plotP}
\title{Plot the P Matrix}
\usage{
plotP(Pmean, Psd = NULL)
}
\arguments{
\item{Pmean}{matrix of mean values of P}

\item{Psd}{matrix of standard deviation values of P}
}
\value{
plot
}
\description{
Plot the P Matrix
}
\details{
plots the P matrix in a line plot with error bars
}
\examples{
data(SimpSim)
plotP(SimpSim.result$Pmean, SimpSim.result$Psd)
}


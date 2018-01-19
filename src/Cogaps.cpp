#include "GibbsSampler.h"
#include "Matrix.h"
#include "Archive.h"
#include "InternalState.h"

#include <Rcpp.h>
#include <ctime>
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

// no C++11 std::to_string
#include <sstream>
#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

#define ARCHIVE_MAGIC_NUM 0xCE45D32A

namespace bpt = boost::posix_time;

static bpt::ptime lastCheckpoint;

static void createCheckpoint(GapsInternalState &state)
{
    state.numCheckpoints++;
    Rcpp::Rcout << "creating gaps checkpoint...";
    bpt::ptime start = bpt::microsec_clock::local_time();
    Archive ar("gaps_checkpoint_" + SSTR(state.numCheckpoints) + ".out",
        ARCHIVE_WRITE);
    ar << ARCHIVE_MAGIC_NUM;
    gaps::random::save(ar);
    ar << state.nEquil;
    ar << state.nSample;
    ar << state.sampler.nRow();
    ar << state.sampler.nCol();
    ar << state.sampler.nFactor();
    ar << state;
    ar.close();
    bpt::time_duration diff = bpt::microsec_clock::local_time() - start;
    double elapsed = diff.total_milliseconds() / 1000.;
    Rcpp::Rcout << " finished in " << elapsed << " seconds\n";
}

static void runGibbsSampler(GapsInternalState &state, unsigned nIterTotal,
Vector &chi2Vec, Vector &aAtomVec, Vector &pAtomVec)
{
    for (; state.iter < nIterTotal; ++state.iter)
    {
        bpt::ptime now = bpt::microsec_clock::local_time();
        bpt::time_duration diff = now - lastCheckpoint;
        if (state.checkpointInterval > 0 && diff.total_milliseconds() > state.checkpointInterval * 1000)
        {
            createCheckpoint(state);
            lastCheckpoint = bpt::microsec_clock::local_time();
        }

        if (state.phase == GAPS_CALIBRATION)
        {
            state.sampler.setAnnealingTemp(std::min(1.0,
                ((float)state.iter + 2.0) / ((float)state.nEquil / 2.0)));
        }

        for (unsigned j = 0; j < state.nIterA; ++j)
        {
            state.sampler.update('A');
        }

        for (unsigned j = 0; j < state.nIterP; ++j)
        {
            state.sampler.update('P');
        }

        if (state.phase == GAPS_SAMPLING)
        {
            state.sampler.updateStatistics();
            if (state.nSnapshots && !((state.iter + 1) % (nIterTotal / state.nSnapshots)))
            {
                state.snapshotsA.push_back(state.sampler.getNormedMatrix('A'));
                state.snapshotsP.push_back(state.sampler.getNormedMatrix('P'));
            }
        }

        if (state.phase != GAPS_COOLING)
        {
            aAtomVec[state.iter] = state.sampler.totalNumAtoms('A');
            pAtomVec[state.iter] = state.sampler.totalNumAtoms('P');
            chi2Vec[state.iter] = state.sampler.chi2();
            state.nIterA = gaps::random::poisson(std::max(aAtomVec[state.iter], 10.f));
            state.nIterP = gaps::random::poisson(std::max(pAtomVec[state.iter], 10.f));

            if ((state.iter + 1) % state.nOutputs == 0 && state.messages)
            {
                std::string temp = state.phase == GAPS_CALIBRATION ? "Equil: " : "Samp: ";
                Rcpp::Rcout << temp << state.iter + 1 << " of " << nIterTotal
                    << ", Atoms:" << aAtomVec[state.iter] << "(" << pAtomVec[state.iter]
                    << ") Chi2 = " << state.sampler.chi2() << '\n';
            }
        }
    }
}

static Rcpp::List runCogaps(GapsInternalState &state)
{
    // reset the checkpoint timer
    lastCheckpoint = bpt::microsec_clock::local_time();

    if (state.phase == GAPS_CALIBRATION)
    {
        runGibbsSampler(state, state.nEquil, state.chi2VecEquil,
            state.nAtomsAEquil, state.nAtomsPEquil);
        state.iter = 0;
        state.phase = GAPS_COOLING;
    }

    if (state.phase == GAPS_COOLING)
    {
        Vector trash(1);
        runGibbsSampler(state, state.nEquilCool, trash, trash, trash);
        state.iter = 0;
        state.phase = GAPS_SAMPLING;
    }

    if (state.phase == GAPS_SAMPLING)
    {
        runGibbsSampler(state, state.nSample, state.chi2VecSample,
            state.nAtomsASample, state.nAtomsPSample);
    }

    // combine chi2 vectors
    Vector chi2Vec(state.chi2VecEquil);
    chi2Vec.concat(state.chi2VecSample);

    return Rcpp::List::create(
        Rcpp::Named("Amean") = state.sampler.AMeanRMatrix(),
        Rcpp::Named("Asd") = state.sampler.AStdRMatrix(),
        Rcpp::Named("Pmean") = state.sampler.PMeanRMatrix(),
        Rcpp::Named("Psd") = state.sampler.PStdRMatrix(),
        Rcpp::Named("ASnapshots") = Rcpp::wrap(state.snapshotsA),
        Rcpp::Named("PSnapshots") = Rcpp::wrap(state.snapshotsP),
        Rcpp::Named("atomsAEquil") = state.nAtomsAEquil.rVec(),
        Rcpp::Named("atomsASamp") = state.nAtomsASample.rVec(),
        Rcpp::Named("atomsPEquil") = state.nAtomsPEquil.rVec(),
        Rcpp::Named("atomsPSamp") = state.nAtomsPSample.rVec(),
        Rcpp::Named("chiSqValues") = chi2Vec.rVec(),
        Rcpp::Named("randSeed") = state.seed
    );
}

// [[Rcpp::export]]
Rcpp::List cogaps(Rcpp::NumericMatrix DMatrix, Rcpp::NumericMatrix SMatrix,
unsigned nFactor, float alphaA, float alphaP, unsigned nEquil,
unsigned nEquilCool, unsigned nSample, float maxGibbsMassA,
float maxGibbsMassP, Rcpp::NumericMatrix fixedPatterns,
char whichMatrixFixed, int seed, bool messages, bool singleCellRNASeq,
unsigned numOutputs, unsigned numSnapshots, unsigned checkpointInterval)
{
    // set seed
    uint32_t seedUsed = static_cast<uint32_t>(seed);
    if (seed < 0)
    {
        bpt::ptime epoch(boost::gregorian::date(1970,1,1)); 
        bpt::ptime now = boost::posix_time::microsec_clock::local_time();
        bpt::time_duration diff = now - epoch;
        seedUsed = static_cast<uint32_t>(diff.total_milliseconds() % 1000);
    }
    gaps::random::setSeed(seedUsed);

    // create internal state from parameters
    GapsInternalState state(DMatrix, SMatrix, nFactor, alphaA, alphaP,
        nEquil, nEquilCool, nSample, maxGibbsMassA, maxGibbsMassP,
        fixedPatterns, whichMatrixFixed, messages, singleCellRNASeq,
        numOutputs, numSnapshots, seedUsed, checkpointInterval);

    // run cogaps from this internal state
    return runCogaps(state);
}

// [[Rcpp::export]]
Rcpp::List cogapsFromCheckpoint(const std::string &fileName)
{   
    // open file
    Archive ar(fileName, ARCHIVE_READ);

    // verify magic number
    uint32_t magicNum = 0;
    ar >> magicNum;
    if (magicNum != ARCHIVE_MAGIC_NUM)
    {
        Rcpp::Rcout << "invalid checkpoint file" << std::endl;
        return Rcpp::List::create();
    }
    
    // seed random number generator and create internal state
    gaps::random::load(ar);

    // read needed parameters
    unsigned nE = 0, nS = 0, nRow = 0, nCol = 0, nFactor = 0;
    ar >> nE;
    ar >> nS;
    ar >> nRow;
    ar >> nCol;
    ar >> nFactor;
    
    // construct empty state of the correct size, populate from file
    GapsInternalState state(nE, nS, nRow, nCol, nFactor);
    ar >> state;

    // run cogaps from this internal state
    return runCogaps(state);
}

#ifndef __COGAPS_GAPS_STATISTICS_H__
#define __COGAPS_GAPS_STATISTICS_H__

#include "GibbsSampler.h"
#include "data_structures/Matrix.h"

enum PumpThreshold
{
    PUMP_UNIQUE=1,
    PUMP_CUT=2
};

class GapsStatistics
{
private:

    ColMatrix mAMeanMatrix;
    ColMatrix mAStdMatrix;
    RowMatrix mPMeanMatrix;
    RowMatrix mPStdMatrix;
    
    unsigned mStatUpdates;
    unsigned mNumPatterns;

    ColMatrix mPumpMatrix;
    PumpThreshold mPumpThreshold;
    unsigned mPumpStatUpdates;

public:

    GapsStatistics(unsigned nRow, unsigned nCol, unsigned nFactor);

    Rcpp::NumericMatrix AMean() const;
    Rcpp::NumericMatrix AStd() const;
    Rcpp::NumericMatrix PMean() const;
    Rcpp::NumericMatrix PStd() const;

    float meanChiSq(const AmplitudeGibbsSampler &ASampler) const;

    void update(const AmplitudeGibbsSampler &ASampler,
        const PatternGibbsSampler &PSampler);

    void updatePump(const AmplitudeGibbsSampler &ASampler,
        const PatternGibbsSampler &PSampler);

    // serialization
    friend Archive& operator<<(Archive &ar, GapsStatistics &stat);
    friend Archive& operator>>(Archive &ar, GapsStatistics &stat);
};

#endif
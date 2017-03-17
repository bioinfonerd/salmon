#ifndef EQUIVALENCE_CLASS_BUILDER_HPP
#define EQUIVALENCE_CLASS_BUILDER_HPP

#include <unordered_map>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>

// Logger includes
#include "spdlog/spdlog.h"

#include "cuckoohash_map.hh"
#include "concurrentqueue.h"
#include "SalmonUtils.hpp"
#include "TranscriptGroup.hpp"
#include "SalmonOpts.hpp"

struct TGValue {
    TGValue(const TGValue& o) {
        weights = o.weights;
	//:posWeights = o.posWeights;
	combinedWeights = o.combinedWeights;
        count.store(o.count.load());
	allWeights = o.allWeights;
    }

    TGValue(std::vector<double>& weightIn, 
	    //std::vector<double>& posWeightsIn, 
	    uint64_t countIn) :
        weights(weightIn.begin(), weightIn.end())
	/*posWeights(posWeightsIn.begin(), posWeightsIn.end())*/ { 
	  count.store(countIn); 
	}

    // const is a lie
    void normalizeAux() const {
      double sumOfAux{0.0};
      for (size_t i = 0; i < weights.size(); ++i) {
	sumOfAux += weights[i];
      }
      double norm = 1.0 / sumOfAux;
      for (size_t i = 0; i < weights.size(); ++i) {
	weights[i].store(weights[i].load() * norm);

      }

      // If we have positional weights, normalize them.
      /*if (posWeights.size() > 0) {
	double posNorm = 1.0 / count.load();
	for (size_t i = 0; i < posWeights.size(); ++i) {
	  posWeights[i].store(posWeights[i].load() * posNorm);
	}
      }*/
    }

    mutable std::vector<tbb::atomic<double>> weights;
    //mutable std::vector<tbb::atomic<double>> posWeights;
    // The combined auxiliary and position weights.  These
    // are filled in by the inference algorithm.
    mutable std::vector<double> combinedWeights;
    std::atomic<uint64_t> count{0};
    std::vector<double> allWeights;
    //std::vector<std::vector<double> > allWeights;
};

class EquivalenceClassBuilder {
    public:
        EquivalenceClassBuilder(std::shared_ptr<spdlog::logger> loggerIn) :
		logger_(loggerIn) {
            countMap_.reserve(1000000);
        }

  //~EquivalenceClassBuilder() {}

        void start() { active_ = true; }

        bool finish() {
            active_ = false;
            size_t totalCount{0};
            auto lt = countMap_.lock_table();
            for (auto& kv : lt) {
                kv.second.normalizeAux();
                totalCount += kv.second.count;
                countVec_.push_back(kv);
            }

    	    logger_->info("Computed {} rich equivalence classes "
			  "for further processing", countVec_.size());
            logger_->info("Counted {} total reads in the equivalence classes ",
                    totalCount);
            return true;
        }

        inline void addGroup(TranscriptGroup&& g,
                             std::vector<double>& weights,
			     //std::vector<double>& posWeights,
			     const SalmonOpts& salmonOpts) {

            auto upfn = [&weights, &salmonOpts](TGValue& x) -> void {
                // update the count
                x.count++;
                // update the weights

		// If we have positional weights
		/*if (weights.size() == posWeights.size()) {
		  for (size_t i = 0; i < x.weights.size(); ++i) {
		    salmon::utils::incLoop(x.weights[i], weights[i]);
		    //salmon::utils::incLoop(x.posWeights[i], posWeights[i]);
		  }
		}*/// else {
	        // With no positional weights
		  for (size_t i = 0; i < x.weights.size(); ++i) {
		    salmon::utils::incLoop(x.weights[i], weights[i]);
		  //}
		}
		if(salmonOpts.useFMEMOpt){
			//x.allWeights.push_back(weights);
			x.allWeights.insert(x.allWeights.end(), weights.begin(), weights.end() );
		}
            };
            TGValue v(weights, 1);
	    if(salmonOpts.useFMEMOpt){
	    	//v.allWeights.push_back(weights);
		v.allWeights.insert(v.allWeights.end(),weights.begin(),weights.end());
	    }
            countMap_.upsert(g, upfn, v);
        }

        std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec() {
            return countVec_;
        }

    private:
        std::atomic<bool> active_;
	    cuckoohash_map<TranscriptGroup, TGValue, TranscriptGroupHasher> countMap_;
        std::vector<std::pair<const TranscriptGroup, TGValue>> countVec_;
    	std::shared_ptr<spdlog::logger> logger_;
};

#endif // EQUIVALENCE_CLASS_BUILDER_HPP

/** Unordered map implementation */
//std::unordered_map<TranscriptGroup, TGValue, TranscriptGroupHasher> countMap_;
//std::mutex mapMut_;
/*
bool finish() {
    // unordered_map implementation
    for (auto& kv : countMap_) {
        kv.second.normalizeAux();
        countVec_.push_back(kv);
    }
    return true;
}
*/

/*
inline void addGroup(TranscriptGroup&& g,
        std::vector<double>& weights) {

    // unordered_map implementation
    std::lock_guard<std::mutex> lock(mapMut_);
    auto it = countMap_.find(g);
    if (it == countMap_.end()) {
        TGValue v(weights, 1);
        countMap_.emplace(g, v);
    } else {
        auto& x = it->second;
        x.count++;
        for (size_t i = 0; i < x.weights.size(); ++i) {
            x.weights[i] =
                salmon::math::logAdd(x.weights[i], weights[i]);
        }
    }
}
*/

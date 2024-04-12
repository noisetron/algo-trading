/*
    Copyright (C) 2019-2024 Noisetron LLC, Bert Schiettecatte 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef DBOSTORE_H 
#define DBOSTORE_H 

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

typedef unsigned long long int PriceType;
static const int numFrac = 12;

inline PriceType convertToPrice(unsigned int a, unsigned int nfrac) { 
	PriceType result = a; 
	while (nfrac--) { 
		result *= 10; 
	}
	return result; 
}

enum OrderSide { 
	osAsk,
	osBid,
};

// code uses boost::multi_index 
// tags are used to access various views on the orders in our 
// depth-by-order book class below. 
namespace tags
{
	struct UpdateTypeAscPriceAsc {}; 
	struct UpdateTypeAscPriceDesc {}; 
	struct IdUnordered {}; 
} 

// DboRecords are not removed from the DboStore. 
// the reason for that is that we need to handle a situation where 
// initially we receive a realtime D before an image N or C (initial book)
// in this case we have to ignore the image N or C which comes after the
// realtime D. we use a composite key below to keep NC updates at the 
// front of the index with the D updates as the back. 
// so we sort ascending on update type, followed by ascending/descending 
// on price.
enum UpdateType
{
	utNewOrChanged, 
	utDeleted, 
}; 

// type used internally by our multi_index below 
struct DboRecord 
{ 
	DboRecord(
		PriceType _price, 
		double _size, 
		int _nsecs, 
		int _ssboe, 
		int _usecs, 
		const std::string& _id, 
		uint64_t _priority, 
		bool _fresh,
		UpdateType _ut) : 

		price(_price), 
		size(_size), 
		nsecs(_nsecs), 
		ssboe(_ssboe), 
		usecs(_usecs), 
		id(_id), 
		priority(_priority), 
		fresh(_fresh), 
	      	ut(_ut) 	
	{
	}

	PriceType price; 
	double size; 
	int nsecs;
	int ssboe;
	int usecs;
	std::string id;
	uint64_t priority; 
	bool fresh; 
	UpdateType ut; 
}; 

using namespace ::boost;
using namespace ::boost::multi_index;

template<typename T> 
class DboStore
{
private: 
	boost::multi_index_container<
		DboRecord,
		indexed_by<
			ordered_non_unique<
				tag<T>,
				composite_key<
					DboRecord,
					member<DboRecord, UpdateType, &DboRecord::ut>, 
					member<DboRecord, PriceType, &DboRecord::price>
				>, 
				composite_key_compare<
					std::less<UpdateType>, 
					std::less<PriceType>
				>
			>,
			hashed_unique<
				tag<tags::IdUnordered>,
				member<DboRecord, std::string, &DboRecord::id>,
				std::hash<std::string> 
			>
		>
	> _recs;

public: 
	void printOrders(int numOrders) {  

		auto&& view = _recs.template get<T>(); 
		auto it = view.begin(); 
		for (int i=0; i<numOrders && it!=view.end(); i++, it++) { 
			std::cout 
				<< "\t" << (it->ut == utNewOrChanged ? "NC" : "D") 
				<< "\t" << (double)(it->price)/convertToPrice(1, numFrac) 
				<< "\t" << it->size 
				<< std::endl;
		}
	}

	void getTopEntries(
		int nTopEntries, 
		std::map<PriceType, double>& m) { 

		PriceType currentPrice = 0;  
		double volume = 0; 
		int numEntries = 0; 
		m.clear(); 

		auto&& view = _recs.template get<T>(); 
		for (auto&& a : view) { 
			if (numEntries >= nTopEntries) {  
				break; 
			} else if (a.ut == utDeleted) { 
				break; 
			} else if (a.price != currentPrice) { 	
				if (currentPrice) { 
					m[currentPrice] = volume; 
					numEntries++; 
				}
				currentPrice = a.price; 
				volume = a.size; 
			} else { 
				volume += a.size; 
			}
		}

		if (numEntries < nTopEntries && currentPrice)  
			m[currentPrice] = volume; 

	}

	void addOrUpdateDirect(
		PriceType price, double size, 
		int nsecs, int ssboe, int usecs, 
		const std::string& id, uint64_t priority, 
		bool fresh, UpdateType ut) { 

		auto&& view = _recs.template get<tags::IdUnordered>();
		auto it = view.find(id);  
		if (it == view.end()) { 
			// order does not exist, insert in-place 
			_recs.emplace(price, size, 
				nsecs, ssboe, usecs, 
				id, priority, fresh, ut);
		} else { 
			// order exists, try modifying it 
			view.modify(it, 
				[price, size, nsecs, ssboe, usecs, 
					id, priority, fresh, ut](DboRecord& r) { 
				// avoid overwriting realtime data with 
				// image data. this will also make sure 
				// that if we receive a realtime D and 
				// then an image C or N, that the realtime 
				// D will stick. 
				if (r.fresh && !fresh) 
					return; 
				// this should never happen, but for sanity 
				// reasons we check this anyway. cannot 
				// change an order back from deleted to 
				// C or N.  
				if (r.ut == utDeleted && ut != utDeleted) { 
					throw std::runtime_error( 
						"order is marked as deleted"); 
				}
				r.ut = ut; 
				r.price = price; 
				r.size = size; 
				r.nsecs = nsecs;
				r.ssboe = ssboe;
				r.usecs = usecs; 
				r.priority = priority; 
				r.fresh = fresh; 
			}); 
		}
	}
}; 

// class to maintain depth-by-order book 
class DboBook
{
private: 
	DboStore<tags::UpdateTypeAscPriceAsc> _asks; 
       	DboStore<tags::UpdateTypeAscPriceDesc> _bids; 

public: 
	void printTopAsks(int nTopEntries) { 

		std::map<PriceType, double> m; 
		getTopEntries(osAsk, nTopEntries, m); 
		printDescending(m); 
	}

	void printTopBids(int nTopEntries) { 

		std::map<PriceType, double> m; 
		getTopEntries(osBid, nTopEntries, m); 
		printDescending(m); 
	}

	void printDescending(std::map<PriceType, double>& m) { 

		for (auto it = m.rbegin(); it != m.rend(); ++it) {
			std::cout
				<< "\t" 
				<< std::to_string(it->first)
				<< "\t\t"
				<< std::to_string(it->second)
				<< std::endl; 
		}
	}

	void printOrders(
		OrderSide s, int numOrders) {  

		if (s == osAsk) { 
			_asks.printOrders(numOrders); 
		} else { 
			_bids.printOrders(numOrders);
		}
	}

	void getTopEntries(
		OrderSide s, 
		int nTopEntries, 
		std::map<PriceType, double>& m) { 

		if (s == osAsk) { 
			_asks.getTopEntries(nTopEntries, m); 
		} else { 
			_bids.getTopEntries(nTopEntries, m); 
		}
	}

	void addOrUpdate(
		OrderSide s, double price, double size, 
		int nsecs, int ssboe, int usecs, 
		const std::string& id, uint64_t priority, 
		bool fresh, UpdateType ut) { 

		if (s == osAsk) { 
			_asks.addOrUpdateDirect(
				price*convertToPrice(1, numFrac), size, 
				nsecs, ssboe, usecs, 
				id, priority, fresh, ut);  
		} else { 
			_bids.addOrUpdateDirect(
				price*convertToPrice(1, numFrac), size, 
				nsecs, ssboe, usecs, 
				id, priority, fresh, ut);  
		}
	}
}; 

#endif

/* This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Copyright (c) 2012-2013 Sergey Lisitsyn, Fernando J. Iglesias Garcia
 */

#ifndef TAPKEE_NEIGHBORS_H_
#define TAPKEE_NEIGHBORS_H_

/* Tapkee includes */
#include <tapkee_defines.hpp>
#ifdef TAPKEE_USE_LGPL_COVERTREE
	#include <neighbors/covertree.hpp>
#endif
#include <neighbors/connected.hpp>
/* End of Tapkee includes */

#include <vector>
#include <utility>
#include <algorithm>

using std::nth_element;
using std::pair;

namespace tapkee
{
namespace tapkee_internal
{

std::string get_neighbors_method_name(TAPKEE_NEIGHBORS_METHOD m)
{
	switch (m)
	{
		case BRUTE_FORCE: return "Brute force";
#ifdef TAPKEE_USE_LGPL_COVERTREE
		case COVER_TREE: return "Cover tree";
#endif
		default: return "Unknown neighbors finding method (yes it is a bug)";
	}
}

template <class DistanceRecord>
struct distances_comparator
{
	bool operator()(const DistanceRecord& l, const DistanceRecord& r) 
	{
		return (l.second < r.second);
	}
};

#ifdef TAPKEE_USE_LGPL_COVERTREE
template <class RandomAccessIterator, class PairwiseCallback>
Neighbors find_neighbors_covertree_impl(RandomAccessIterator begin, RandomAccessIterator end, 
                         PairwiseCallback callback, IndexType k)
{
	timed_context context("Covertree-based neighbors search");

	typedef CoverTreePoint<RandomAccessIterator> TreePoint;
	v_array<TreePoint> points;
	for (RandomAccessIterator iter=begin; iter!=end; ++iter)
		push(points, TreePoint(iter, callback(*iter,*iter)));

	node<TreePoint> ct = batch_create(callback, points);

	v_array< v_array<TreePoint> > res;
	++k; // because one of the neighbors will be the actual query point
	k_nearest_neighbor(callback,ct,ct,res,k);

	Neighbors neighbors;
	neighbors.resize(end-begin);
	assert(end-begin==res.index);
	for (int i=0; i<res.index; ++i)
	{
		LocalNeighbors local_neighbors;
		local_neighbors.reserve(k);
		
		for (IndexType j=1; j<=k; ++j) // j=0 is the query point
		{
			// The actual query point is found as a neighbor, just ignore it
			if (res[i][j].iter_-begin==res[i][0].iter_-begin)
				continue;
			local_neighbors.push_back(res[i][j].iter_-begin);
		}
		neighbors[res[i][0].iter_-begin] = local_neighbors;
		free(res[i].elements);
	};
	free(res.elements);
	free_children(ct);
	free(points.elements);
	return neighbors;
}
#endif

template <class RandomAccessIterator, class PairwiseCallback>
Neighbors find_neighbors_bruteforce_impl(const RandomAccessIterator& begin, const RandomAccessIterator& end, 
                                         PairwiseCallback callback, IndexType k)
{
	timed_context context("Distance sorting based neighbors search");
	typedef std::pair<RandomAccessIterator, ScalarType> DistanceRecord;
	typedef std::vector<DistanceRecord> Distances;

	Neighbors neighbors;
	neighbors.reserve(end-begin);
	for (RandomAccessIterator iter=begin; iter!=end; ++iter)
	{
		Distances distances;
		if (BasicCallbackTraits<PairwiseCallback>::is_kernel)
		{
			for (RandomAccessIterator around_iter=begin; around_iter!=end; ++around_iter)
				distances.push_back(make_pair(around_iter, callback(*around_iter,*around_iter) + callback(*iter,*iter) - 2*callback(*iter,*around_iter)));
		}
		else
		{
			for (RandomAccessIterator around_iter=begin; around_iter!=end; ++around_iter)
				distances.push_back(make_pair(around_iter, callback(*iter,*around_iter)));
		}

		nth_element(distances.begin(),distances.begin()+k+1,distances.end(),
		            distances_comparator<DistanceRecord>());

		LocalNeighbors local_neighbors;
		local_neighbors.reserve(k);
		for (typename Distances::const_iterator neighbors_iter=distances.begin(); 
				neighbors_iter!=distances.begin()+k+1; ++neighbors_iter)
		{
			if (neighbors_iter->first != iter) 
				local_neighbors.push_back(neighbors_iter->first - begin);
		}
		neighbors.push_back(local_neighbors);
	}
	return neighbors;
}

template <class RandomAccessIterator, class PairwiseCallback>
Neighbors find_neighbors(TAPKEE_NEIGHBORS_METHOD method, const RandomAccessIterator& begin, 
                         const RandomAccessIterator& end, const PairwiseCallback& callback, 
                         IndexType k, bool check_connectivity)
{
	if (k > static_cast<IndexType>(end-begin-1))
	{
		LoggingSingleton::instance().message_warning("Number of neighbors is greater than number of objects to embed. "
		                                             "Using greatest possible number of neighbors.");
		k = static_cast<IndexType>(end-begin-1);
	}
	LoggingSingleton::instance().message_info("Using " + get_neighbors_method_name(method) + " neighbors computation method.");
	Neighbors neighbors;
	switch (method)
	{
		case BRUTE_FORCE: neighbors = find_neighbors_bruteforce_impl(begin,end,callback,k); break;
#ifdef TAPKEE_USE_LGPL_COVERTREE
		case COVER_TREE: neighbors = find_neighbors_covertree_impl(begin,end,callback,k); break;
#endif
		default: break;
	}

	if (check_connectivity)
	{
		timed_context connectivity("Checking connectivity");
		if (!is_connected(begin,end,neighbors))
			LoggingSingleton::instance().message_warning("The neighborhood graph is not connected.");
	}
	return neighbors;
}

} // End of namespace tapkee
} // End of namespace tapkee_internal

#endif
